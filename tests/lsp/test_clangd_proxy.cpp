#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <iostream>
#include <sstream>
#include <string>
#include "weasel/lsp/jsonrpc.hpp"
#include "weasel/lsp/server.hpp"

using weasel::lsp::json;
using weasel::lsp::server;

namespace {

void append_framed(std::string& out, const json& msg) {
    std::string body = msg.dump();
    out += "Content-Length: ";
    out += std::to_string(body.size());
    out += "\r\n\r\n";
    out += body;
}

std::vector<json> parse_all_framed(const std::string& s) {
    std::vector<json> out;
    std::istringstream iss(s);
    while (true) {
        auto msg = weasel::lsp::read_message(iss);
        if (!msg)
            break;
        out.push_back(*msg);
    }
    return out;
}

json make_request(int id, std::string_view method, json params = json::object()) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"method", std::string(method)}, {"params", std::move(params)}};
}

json make_notif(std::string_view method, json params) {
    return {{"jsonrpc", "2.0"}, {"method", std::string(method)}, {"params", std::move(params)}};
}

std::vector<json> drive(const std::vector<json>& inputs) {
    std::string raw_in;
    for (const auto& m : inputs)
        append_framed(raw_in, m);
    std::istringstream in(raw_in);
    std::ostringstream out;
    server srv(in, out);
    srv.run();
    return parse_all_framed(out.str());
}

}  // namespace

TEST_CASE("clangd proxy: diagnostics from mock clangd are remapped to .weasel URI") {
    // This test runs only when mock_clangd is available on PATH via
    // WEASEL_CLANGD_PATH. The build sets this for the test.
    auto msgs = drive({
        make_request(1, "initialize", {{"rootUri", nullptr}}),
        make_notif("textDocument/didOpen",
                   {
                       {"textDocument",
                        {
                            {"uri", "file:///tmp/proxy.weasel"},
                            {"languageId", "weasel"},
                            {"version", 1},
                            {"text",
                             "node f() {\n"        // cc line 2 (line 1 is auto-preamble)
                             "  int x = 0;\n"      // cc line 3
                             "  return <div/>;\n"  // cc line 4  (ccx region)
                             "}\n"},
                        }},
                   }),
        make_request(2, "shutdown"),
        make_notif("exit", json::object()),
    });

    // Collect all publishDiagnostics.
    bool saw_clangd_diag = false;
    for (const auto& m : msgs) {
        if (m.value("method", "") != "textDocument/publishDiagnostics")
            continue;
        if (m.at("params").value("uri", "") != "file:///tmp/proxy.weasel")
            continue;
        for (const auto& d : m.at("params").at("diagnostics")) {
            if (d.value("source", "") == "mock-clangd") {
                saw_clangd_diag = true;
                int line = d.at("range").at("start").value("line", -1);
                CHECK(line == 0);  // mock emits at cc line 2 (0-indexed 1) = weasel line 1 (0-indexed 0)
            }
        }
    }
    if (!saw_clangd_diag) {
        std::cerr << "DEBUG messages received:\n";
        for (const auto& m : msgs)
            std::cerr << "  " << m.dump() << "\n";
    }
    CHECK(saw_clangd_diag);
}
