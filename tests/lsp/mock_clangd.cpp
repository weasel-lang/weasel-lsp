// Canned "clangd" for LSP proxy testing. Reads LSP-framed JSON messages from
// stdin and emits scripted responses on stdout.
//
// Behavior:
// - initialize request -> returns a result with minimal capabilities.
// - shutdown request -> returns null.
// - textDocument/didOpen notification -> publishes a canned diagnostic on
//   the .cc URI. The diagnostic lives on .cc line 2 so the proxy can remap
//   it (tests should build their input so .cc line 2 lands in a
//   cpp_passthrough span OR a ccx_region span).
// - Any request -> replies with result = null.
// - exit notification -> exits cleanly.

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <nlohmann/json.hpp>
#include <string>
#include <strings.h>

using json = nlohmann::json;

static std::string read_line() {
    std::string line;
    int c;
    while ((c = std::fgetc(stdin)) != EOF) {
        line.push_back(static_cast<char>(c));
        if (line.size() >= 2 && line[line.size() - 2] == '\r' && line.back() == '\n') break;
    }
    return line;
}

static bool read_message(json& out) {
    size_t content_length = 0;
    while (true) {
        auto line = read_line();
        if (line.empty()) return false;
        if (line == "\r\n") break;
        const char* prefix = "Content-Length:";
        size_t plen = std::strlen(prefix);
        if (line.size() > plen && strncasecmp(line.data(), prefix, plen) == 0) {
            size_t i = plen;
            while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
            content_length = std::strtoull(line.c_str() + i, nullptr, 10);
        }
    }
    if (content_length == 0) { out = json::object(); return true; }
    std::string body(content_length, '\0');
    size_t got = 0;
    while (got < content_length) {
        int c = std::fgetc(stdin);
        if (c == EOF) return false;
        body[got++] = static_cast<char>(c);
    }
    try { out = json::parse(body); return true; } catch (...) { return false; }
}

static void write_message(const json& msg) {
    std::string body = msg.dump();
    std::fprintf(stdout, "Content-Length: %zu\r\n\r\n", body.size());
    std::fwrite(body.data(), 1, body.size(), stdout);
    std::fflush(stdout);
}

int main() {
    json msg;
    while (read_message(msg)) {
        std::string method = msg.value("method", "");
        bool has_id = msg.contains("id");
        if (method == "initialize" && has_id) {
            write_message({
                {"jsonrpc", "2.0"}, {"id", msg.at("id")},
                {"result", {{"capabilities", json::object()}}},
            });
            continue;
        }
        if (method == "shutdown" && has_id) {
            write_message({{"jsonrpc", "2.0"}, {"id", msg.at("id")}, {"result", nullptr}});
            continue;
        }
        if (method == "exit") {
            return 0;
        }
        if (method == "textDocument/didOpen" || method == "textDocument/didChange") {
            // Emit a diagnostic on line 1 (cc) — the proxy should remap it to
            // the corresponding .weasel line for the file.
            std::string uri = msg.at("params").at("textDocument").at("uri").get<std::string>();
            json diag = {
                {"range", {
                    {"start", {{"line", 1}, {"character", 5}}},
                    {"end",   {{"line", 1}, {"character", 10}}},
                }},
                {"severity", 1},
                {"source", "mock-clangd"},
                {"message", "canned clangd error"},
            };
            write_message({
                {"jsonrpc", "2.0"},
                {"method", "textDocument/publishDiagnostics"},
                {"params", {{"uri", uri}, {"diagnostics", json::array({diag})}}},
            });
            continue;
        }
        if (has_id) {
            // Generic reply for unknown requests.
            write_message({{"jsonrpc", "2.0"}, {"id", msg.at("id")}, {"result", nullptr}});
            continue;
        }
    }
    return 0;
}
