#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <sstream>
#include <string>
#include "weasel/lsp/jsonrpc.hpp"
#include "weasel/lsp/server.hpp"

using weasel::lsp::json;
using weasel::lsp::server;

namespace {

// Serialize a JSON-RPC message with Content-Length framing into `out`.
void append_framed(std::string& out, const json& msg) {
    std::string body = msg.dump();
    out += "Content-Length: ";
    out += std::to_string(body.size());
    out += "\r\n\r\n";
    out += body;
}

// Parse all framed messages out of a string.
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

TEST_CASE("initialize → initialize response with capabilities") {
    auto msgs = drive({
        make_request(1, "initialize", {{"processId", nullptr}, {"rootUri", nullptr}}),
        make_request(2, "shutdown"),
        make_notif("exit", json::object()),
    });
    REQUIRE(msgs.size() >= 2);
    // First must be the initialize response
    CHECK(msgs[0].at("id") == 1);
    REQUIRE(msgs[0].contains("result"));
    const auto& caps = msgs[0].at("result").at("capabilities");
    CHECK(caps.at("definitionProvider") == true);
    CHECK(caps.contains("completionProvider"));
    CHECK(caps.at("textDocumentSync") == 1);
}

TEST_CASE("didOpen publishes diagnostics (success: empty list)") {
    auto msgs = drive({
        make_request(1, "initialize", json::object()),
        make_notif("textDocument/didOpen",
                   {
                       {"textDocument",
                        {
                            {"uri", "file:///tmp/hello.weasel"},
                            {"languageId", "weasel"},
                            {"version", 1},
                            {"text", "node f() { return <p>hi</p>; }\n"},
                        }},
                   }),
        make_request(2, "shutdown"),
        make_notif("exit", json::object()),
    });
    // Find the publishDiagnostics notification
    bool saw_diag = false;
    for (const auto& m : msgs) {
        if (m.value("method", "") == "textDocument/publishDiagnostics") {
            saw_diag = true;
            CHECK(m.at("params").at("uri") == "file:///tmp/hello.weasel");
            CHECK(m.at("params").at("diagnostics").is_array());
            CHECK(m.at("params").at("diagnostics").size() == 0);
        }
    }
    CHECK(saw_diag);
}

TEST_CASE("didOpen with parse error publishes diagnostic with position") {
    auto msgs = drive({
        make_request(1, "initialize", json::object()),
        make_notif("textDocument/didOpen",
                   {
                       {"textDocument",
                        {
                            {"uri", "file:///tmp/bad.weasel"},
                            {"languageId", "weasel"},
                            {"version", 1},
                            {"text", "node f() { return <div></span>; }\n"},
                        }},
                   }),
        make_request(2, "shutdown"),
        make_notif("exit", json::object()),
    });
    json found;
    for (const auto& m : msgs) {
        if (m.value("method", "") == "textDocument/publishDiagnostics") {
            found = m;
        }
    }
    REQUIRE(!found.is_null());
    const auto& diags = found.at("params").at("diagnostics");
    REQUIRE(diags.is_array());
    REQUIRE(diags.size() == 1);
    CHECK(diags[0].at("severity") == 1);
    CHECK(diags[0].at("source") == "weasel");
    CHECK(diags[0].at("range").at("start").at("line") == 0);
}

TEST_CASE("textDocument/definition jumps to component") {
    auto msgs = drive({
        make_request(1, "initialize", json::object()),
        make_notif("textDocument/didOpen",
                   {
                       {"textDocument",
                        {
                            {"uri", "file:///tmp/d.weasel"},
                            {"languageId", "weasel"},
                            {"version", 1},
                            {"text",
                             "component MyCard(int) { return <div/>; }\n"
                             "node page() { return <MyCard/>; }\n"},
                        }},
                   }),
        // The <MyCard/> is on line 1 (0-based); MyCard starts around column 22.
        make_request(2, "textDocument/definition",
                     {
                         {"textDocument", {{"uri", "file:///tmp/d.weasel"}}},
                         {"position", {{"line", 1}, {"character", 23}}},  // on 'M' of MyCard
                     }),
        make_request(3, "shutdown"),
        make_notif("exit", json::object()),
    });
    json def;
    for (const auto& m : msgs) {
        if (m.value("id", -1) == 2)
            def = m;
    }
    REQUIRE(!def.is_null());
    const auto& result = def.at("result");
    REQUIRE(!result.is_null());
    CHECK(result.at("uri") == "file:///tmp/d.weasel");
    CHECK(result.at("range").at("start").at("line") == 0);
}

TEST_CASE("textDocument/completion offers CCX tags and components inside CCX") {
    auto msgs = drive({
        make_request(1, "initialize", json::object()),
        make_notif("textDocument/didOpen",
                   {
                       {"textDocument",
                        {
                            {"uri", "file:///tmp/c.weasel"},
                            {"languageId", "weasel"},
                            {"version", 1},
                            {"text",
                             "component MyCard(int) { return <div/>; }\n"
                             "node page() { return <div>hello</div>; }\n"},
                        }},
                   }),
        // Cursor inside the <div>...</div> on line 1
        make_request(2, "textDocument/completion",
                     {
                         {"textDocument", {{"uri", "file:///tmp/c.weasel"}}},
                         {"position", {{"line", 1}, {"character", 25}}},  // somewhere in CCX
                     }),
        make_request(3, "shutdown"),
        make_notif("exit", json::object()),
    });
    json compl_resp;
    for (const auto& m : msgs) {
        if (m.value("id", -1) == 2)
            compl_resp = m;
    }
    REQUIRE(!compl_resp.is_null());
    const auto& items = compl_resp.at("result").at("items");
    REQUIRE(items.is_array());
    CHECK(items.size() > 0);
    bool saw_div = false, saw_mycard = false;
    for (const auto& it : items) {
        if (it.at("label") == "div")
            saw_div = true;
        if (it.at("label") == "MyCard")
            saw_mycard = true;
    }
    CHECK(saw_div);
    CHECK(saw_mycard);
}

TEST_CASE("completion inside CCX expression {…} returns empty not HTML tags") {
    // Without clangd, expression positions should yield nothing (not spurious HTML).
    // Source line 0: node f(int x) { return <div class={x}>hi</div>; }
    // The {x} attribute expression starts at column 34 (0-based).
    auto msgs = drive({
        make_request(1, "initialize", json::object()),
        make_notif("textDocument/didOpen",
                   {
                       {"textDocument",
                        {
                            {"uri", "file:///tmp/expr.weasel"},
                            {"languageId", "weasel"},
                            {"version", 1},
                            {"text", "node f(int x) { return <div class={x}>hi</div>; }\n"},
                        }},
                   }),
        make_request(2, "textDocument/completion",
                     {
                         {"textDocument", {{"uri", "file:///tmp/expr.weasel"}}},
                         {"position", {{"line", 0}, {"character", 35}}},  // on 'x' inside {x}
                     }),
        make_request(3, "shutdown"),
        make_notif("exit", json::object()),
    });
    json resp;
    for (const auto& m : msgs) {
        if (m.value("id", -1) == 2)
            resp = m;
    }
    REQUIRE(!resp.is_null());
    const auto& items = resp.at("result").at("items");
    bool saw_html_tag = false;
    for (const auto& it : items) {
        if (it.value("detail", "") == "HTML element")
            saw_html_tag = true;
    }
    CHECK(!saw_html_tag);
}

TEST_CASE("completion outside CCX is empty (v0.5)") {
    auto msgs = drive({
        make_request(1, "initialize", json::object()),
        make_notif("textDocument/didOpen",
                   {
                       {"textDocument",
                        {
                            {"uri", "file:///tmp/o.weasel"},
                            {"languageId", "weasel"},
                            {"version", 1},
                            {"text", "int f() { return 0; }\n"},
                        }},
                   }),
        make_request(2, "textDocument/completion",
                     {
                         {"textDocument", {{"uri", "file:///tmp/o.weasel"}}},
                         {"position", {{"line", 0}, {"character", 5}}},
                     }),
        make_request(3, "shutdown"),
        make_notif("exit", json::object()),
    });
    json resp;
    for (const auto& m : msgs) {
        if (m.value("id", -1) == 2)
            resp = m;
    }
    REQUIRE(!resp.is_null());
    CHECK(resp.at("result").at("items").empty());
}

TEST_CASE("malformed Content-Length is rejected without crashing") {
    // Oversized Content-Length should return nullopt, not allocate/crash.
    std::string raw = "Content-Length: 9999999999999\r\n\r\n";
    std::istringstream iss(raw);
    auto result = weasel::lsp::read_message(iss);
    CHECK(!result.has_value());
}

TEST_CASE("Content-Length at 64MiB limit is rejected") {
    // Exactly one byte over the 64 MiB cap.
    std::string raw = "Content-Length: " + std::to_string(64u * 1024 * 1024 + 1) + "\r\n\r\n";
    std::istringstream iss(raw);
    auto result = weasel::lsp::read_message(iss);
    CHECK(!result.has_value());
}
