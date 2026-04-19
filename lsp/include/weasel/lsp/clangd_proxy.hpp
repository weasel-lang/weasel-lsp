#pragma once
#include "weasel/lsp/jsonrpc.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace weasel::lsp {

// Spawns and talks to a clangd subprocess over pipes. Owns a reader thread
// that parses LSP-framed messages from clangd's stdout and invokes callbacks.
//
// Threading:
// - send_notification / send_request are safe to call from any thread. They
//   serialize writes to clangd's stdin internally.
// - response_cb and the registered notification handler are invoked on the
//   internal reader thread. Handlers that write to the editor output must
//   serialize via the server's write mutex.
class clangd_proxy {
  public:
    using response_cb = std::function<void(const json& result, const json& error)>;
    using notification_cb = std::function<void(const std::string& method,
                                               const json& params)>;

    // Spawn clangd. Returns nullptr on any failure (no clangd on PATH, pipe
    // creation error, etc.). `compile_commands_dir` is an absolute path passed
    // as --compile-commands-dir; empty means don't pass the flag.
    static std::unique_ptr<clangd_proxy>
    spawn(std::string_view compile_commands_dir = "");

    ~clangd_proxy();

    clangd_proxy(const clangd_proxy&) = delete;
    clangd_proxy& operator=(const clangd_proxy&) = delete;

    // Fire-and-forget notification to clangd.
    void send_notification(std::string_view method, json params);

    // Request with async response via `cb`. `cb` runs on the reader thread.
    void send_request(std::string_view method, json params, response_cb cb);

    // Register the single handler for clangd-initiated notifications.
    // Typical example: textDocument/publishDiagnostics.
    void set_notification_handler(notification_cb cb);

    // Synchronously send shutdown + exit and wait briefly for the process to
    // terminate. Called from server::run teardown.
    void shutdown();

  private:
    clangd_proxy(int stdin_fd, int stdout_fd, int pid);

    void reader_loop();
    void write_to_clangd(const json& msg);

    int stdin_fd_ = -1;
    int stdout_fd_ = -1;
    int pid_ = -1;

    std::mutex write_mutex_;
    std::thread reader_;
    std::atomic<bool> stopping_{false};

    std::atomic<std::int64_t> next_id_{1};
    std::mutex pending_mutex_;
    std::unordered_map<std::int64_t, response_cb> pending_;

    std::mutex handler_mutex_;
    notification_cb notification_handler_;
};

} // namespace weasel::lsp
