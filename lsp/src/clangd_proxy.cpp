#include "weasel/lsp/clangd_proxy.hpp"

#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <spawn.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char** environ;

namespace weasel::lsp {

namespace {
std::ostream& log() { return std::cerr; }

// Read a single LSP-framed message from a raw file descriptor. Returns
// std::nullopt on EOF or error. Blocks until a full message is available.
std::optional<json> read_framed_from_fd(int fd) {
    // Read headers line-by-line until blank line; then read Content-Length bytes.
    std::string headers;
    char c;
    size_t content_length = 0;
    while (true) {
        // Read one line terminated by \r\n.
        std::string line;
        while (true) {
            ssize_t n = ::read(fd, &c, 1);
            if (n <= 0) return std::nullopt;
            line.push_back(c);
            if (line.size() >= 2 && line[line.size() - 2] == '\r' && line.back() == '\n') {
                break;
            }
        }
        if (line == "\r\n") break;  // end of headers
        // Parse "Content-Length: N"
        constexpr std::string_view cl = "Content-Length:";
        if (line.size() > cl.size() &&
            strncasecmp(line.data(), cl.data(), cl.size()) == 0) {
            size_t i = cl.size();
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
            constexpr size_t max_message_size = 64u * 1024 * 1024; // 64 MiB
            size_t v = std::strtoull(line.c_str() + i, nullptr, 10);
            content_length = (v <= max_message_size) ? v : 0;
        }
    }
    if (content_length == 0) return json::object();
    std::string body(content_length, '\0');
    size_t got = 0;
    while (got < content_length) {
        ssize_t n = ::read(fd, body.data() + got, content_length - got);
        if (n <= 0) return std::nullopt;
        got += static_cast<size_t>(n);
    }
    try {
        return json::parse(body);
    } catch (...) {
        return std::nullopt;
    }
}

void write_framed_to_fd(int fd, const json& msg) {
    std::string body = msg.dump();
    std::string header = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
    ::write(fd, header.data(), header.size());
    ::write(fd, body.data(), body.size());
}

std::string find_clangd_path() {
    if (const char* env = std::getenv("WEASEL_CLANGD_PATH")) {
        if (*env) return env;
    }
    return "clangd";
}

// When exe contains a '/' it's an absolute or relative path — we can probe
// it directly. Bare names rely on PATH lookup done by posix_spawnp, so we
// skip the probe (access() wouldn't search PATH).
void maybe_warn_clangd_not_found(const std::string& exe) {
    if (exe.find('/') == std::string::npos) return;
    if (::access(exe.c_str(), X_OK) != 0) {
        log() << "weasel-lsp: clangd not found or not executable at '"
              << exe << "'; will attempt spawn anyway\n";
    }
}

} // namespace

std::unique_ptr<clangd_proxy> clangd_proxy::spawn(std::string_view compile_commands_dir) {
    int to_child[2];   // parent writes to [1], child reads from [0]
    int from_child[2]; // child writes to [1], parent reads from [0]
    if (::pipe(to_child) != 0) return nullptr;
    if (::pipe(from_child) != 0) {
        ::close(to_child[0]);
        ::close(to_child[1]);
        return nullptr;
    }

    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        ::close(to_child[0]); ::close(to_child[1]);
        ::close(from_child[0]); ::close(from_child[1]);
        return nullptr;
    }
    posix_spawn_file_actions_adddup2(&actions, to_child[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, from_child[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, to_child[0]);
    posix_spawn_file_actions_addclose(&actions, to_child[1]);
    posix_spawn_file_actions_addclose(&actions, from_child[0]);
    posix_spawn_file_actions_addclose(&actions, from_child[1]);

    std::string exe = find_clangd_path();
    maybe_warn_clangd_not_found(exe);
    std::vector<std::string> args_storage;
    args_storage.push_back(exe);
    args_storage.push_back("--background-index=false");
    args_storage.push_back("--log=error");
    std::string ccd_flag;
    if (!compile_commands_dir.empty()) {
        ccd_flag = "--compile-commands-dir=" + std::string(compile_commands_dir);
        args_storage.push_back(ccd_flag);
    }
    std::vector<char*> argv;
    for (auto& s : args_storage) argv.push_back(s.data());
    argv.push_back(nullptr);

    pid_t pid = 0;
    int rc = posix_spawnp(&pid, exe.c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);

    // Parent: close the child's ends.
    ::close(to_child[0]);
    ::close(from_child[1]);
    if (rc != 0) {
        log() << "weasel-lsp: clangd spawn failed (" << std::strerror(rc)
              << "); falling back to v0.5 mode without C++ intelligence\n";
        ::close(to_child[1]);
        ::close(from_child[0]);
        return nullptr;
    }

    // Wrap in proxy (private constructor, hence new + reset).
    std::unique_ptr<clangd_proxy> p(new clangd_proxy(to_child[1], from_child[0], pid));
    p->reader_ = std::thread(&clangd_proxy::reader_loop, p.get());
    return p;
}

clangd_proxy::clangd_proxy(int stdin_fd, int stdout_fd, int pid)
    : stdin_fd_(stdin_fd), stdout_fd_(stdout_fd), pid_(pid) {}

clangd_proxy::~clangd_proxy() {
    shutdown();
}

void clangd_proxy::shutdown() {
    bool expected = false;
    if (!stopping_.compare_exchange_strong(expected, true)) return;

    if (stdin_fd_ >= 0) {
        // Use the proxy's request/response flow so we know when clangd has
        // finished processing everything we sent earlier. Any publishDiagnostics
        // from prior didOpen/didChange will arrive BEFORE the shutdown reply
        // (clangd processes messages in order), so by the time we wake up the
        // reader thread has already relayed them to the editor.
        std::mutex m;
        std::condition_variable cv;
        bool done = false;
        try {
            send_request("shutdown", json::object(), [&](const json&, const json&) {
                std::lock_guard<std::mutex> lk(m);
                done = true;
                cv.notify_all();
            });
            std::unique_lock<std::mutex> lk(m);
            cv.wait_for(lk, std::chrono::seconds(2), [&] { return done; });
        } catch (...) {}
        try { send_notification("exit", json::object()); } catch (...) {}
        ::close(stdin_fd_);
        stdin_fd_ = -1;
    }

    if (reader_.joinable()) reader_.join();

    if (pid_ > 0) {
        int status = 0;
        for (int i = 0; i < 200; ++i) {
            pid_t r = ::waitpid(pid_, &status, WNOHANG);
            if (r == pid_) { pid_ = -1; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (pid_ > 0) {
            ::kill(pid_, SIGTERM);
            ::waitpid(pid_, nullptr, 0);
            pid_ = -1;
        }
    }

    if (stdout_fd_ >= 0) {
        ::close(stdout_fd_);
        stdout_fd_ = -1;
    }
}

void clangd_proxy::write_to_clangd(const json& msg) {
    std::lock_guard<std::mutex> lk(write_mutex_);
    if (stdin_fd_ < 0) return;
    write_framed_to_fd(stdin_fd_, msg);
}

void clangd_proxy::send_notification(std::string_view method, json params) {
    json m = {{"jsonrpc", "2.0"}, {"method", std::string(method)},
              {"params", std::move(params)}};
    write_to_clangd(m);
}

void clangd_proxy::send_request(std::string_view method, json params, response_cb cb) {
    std::int64_t id = next_id_.fetch_add(1);
    {
        std::lock_guard<std::mutex> lk(pending_mutex_);
        pending_.emplace(id, std::move(cb));
    }
    json m = {{"jsonrpc", "2.0"}, {"id", id}, {"method", std::string(method)},
              {"params", std::move(params)}};
    write_to_clangd(m);
}

void clangd_proxy::set_notification_handler(notification_cb cb) {
    std::lock_guard<std::mutex> lk(handler_mutex_);
    notification_handler_ = std::move(cb);
}

void clangd_proxy::reader_loop() {
    // Runs until clangd closes its stdout (natural EOF after processing `exit`).
    while (true) {
        auto msg = read_framed_from_fd(stdout_fd_);
        if (!msg) return;

        // Response (has id AND no method, or has id with result/error only)
        if (msg->contains("id") && !msg->contains("method")) {
            std::int64_t id = 0;
            try { id = msg->at("id").get<std::int64_t>(); } catch (...) {
                log() << "weasel-lsp: unexpected id type in clangd response (expected integer); dropping\n";
                continue;
            }
            response_cb cb;
            {
                std::lock_guard<std::mutex> lk(pending_mutex_);
                auto it = pending_.find(id);
                if (it == pending_.end()) continue;
                cb = std::move(it->second);
                pending_.erase(it);
            }
            json result = msg->value("result", json(nullptr));
            json err    = msg->value("error",  json(nullptr));
            try { if (cb) cb(result, err); }
            catch (const std::exception& e) { log() << "weasel-lsp: response cb threw: " << e.what() << "\n"; }
            continue;
        }

        // Notification or server-initiated request (id + method).
        if (msg->contains("method")) {
            std::string method = msg->at("method").get<std::string>();
            json params = msg->value("params", json::object());
            notification_cb handler;
            {
                std::lock_guard<std::mutex> lk(handler_mutex_);
                handler = notification_handler_;
            }
            try { if (handler) handler(method, params); }
            catch (const std::exception& e) { log() << "weasel-lsp: notif cb threw: " << e.what() << "\n"; }
            // Server-initiated requests from clangd (e.g. workspace/configuration):
            // we don't proxy them to the editor in v1. Send a generic null-result
            // reply so clangd isn't stuck waiting.
            if (msg->contains("id")) {
                json reply = {{"jsonrpc", "2.0"}, {"id", msg->at("id")}, {"result", nullptr}};
                write_to_clangd(reply);
            }
            continue;
        }
    }
}

} // namespace weasel::lsp
