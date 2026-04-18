#include "weasel/lsp/server.hpp"
#include "weasel/lsp/features.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace weasel::lsp {

namespace {

std::string derive_cc_path(std::string_view weasel_path) {
    auto dot = weasel_path.find_last_of('.');
    std::string base = (dot == std::string_view::npos)
                           ? std::string(weasel_path)
                           : std::string(weasel_path.substr(0, dot));
    return base + ".cc";
}

// Log to stderr (stdout is reserved for LSP traffic).
std::ostream& log() { return std::cerr; }

} // namespace

server::server(std::istream& in, std::ostream& out) : in_(in), out_(out) {}

int server::run() {
    while (!shutdown_requested_) {
        auto msg = read_message(in_);
        if (!msg) break;
        try {
            handle_message(*msg);
        } catch (const std::exception& e) {
            log() << "weasel-lsp: exception in handler: " << e.what() << "\n";
        }
    }
    return 0;
}

void server::handle_message(const json& msg) {
    if (!msg.contains("method")) return;  // responses to server-initiated reqs
    std::string method = msg.at("method").get<std::string>();
    json params = msg.contains("params") ? msg.at("params") : json::object();
    if (msg.contains("id")) {
        handle_request(msg.at("id"), method, params);
    } else {
        handle_notification(method, params);
    }
}

void server::handle_request(const json& id, const std::string& method, const json& params) {
    if (method == "initialize") {
        write_message(out_, make_response(id, on_initialize(params)));
        return;
    }
    if (method == "shutdown") {
        shutdown_requested_ = true;
        write_message(out_, make_response(id, nullptr));
        return;
    }
    if (method == "textDocument/definition") {
        write_message(out_, make_response(id, on_definition(params)));
        return;
    }
    if (method == "textDocument/completion") {
        write_message(out_, make_response(id, on_completion(params)));
        return;
    }
    write_message(out_, make_error_response(id, -32601, "method not found"));
}

void server::handle_notification(const std::string& method, const json& params) {
    if (method == "initialized") { initialized_ = true; return; }
    if (method == "exit") { shutdown_requested_ = true; return; }
    if (method == "textDocument/didOpen")   { on_did_open(params);   return; }
    if (method == "textDocument/didChange") { on_did_change(params); return; }
    if (method == "textDocument/didSave")   { on_did_save(params);   return; }
    if (method == "textDocument/didClose")  { on_did_close(params);  return; }
    // Unknown notifications: silently ignore per spec.
}

json server::on_initialize(const json& /*params*/) {
    return {
        {"capabilities", {
            {"textDocumentSync", 1},  // full sync
            {"definitionProvider", true},
            {"completionProvider", {
                {"triggerCharacters", json::array({"<", "/", " "})},
            }},
            {"positionEncoding", "utf-8"},
        }},
        {"serverInfo", {
            {"name", "weasel-lsp"},
            {"version", "0.5.0"},
        }},
    };
}

json server::on_definition(const json& params) {
    std::string uri = params.at("textDocument").at("uri").get<std::string>();
    int line = params.at("position").at("line").get<int>();
    int character = params.at("position").at("character").get<int>();
    const doc_state* d = docs_.find(uri);
    if (!d) return nullptr;
    return build_definition(*d, line, character);
}

json server::on_completion(const json& params) {
    std::string uri = params.at("textDocument").at("uri").get<std::string>();
    int line = params.at("position").at("line").get<int>();
    int character = params.at("position").at("character").get<int>();
    const doc_state* d = docs_.find(uri);
    if (!d) return {{"isIncomplete", false}, {"items", json::array()}};
    return build_completion(*d, line, character);
}

void server::on_did_open(const json& params) {
    const auto& td = params.at("textDocument");
    auto& d = docs_.open_or_update(
        td.at("uri").get<std::string>(),
        td.at("text").get<std::string>(),
        td.value("version", 0));
    publish_diagnostics(d);
}

void server::on_did_change(const json& params) {
    std::string uri = params.at("textDocument").at("uri").get<std::string>();
    int version = params.at("textDocument").value("version", 0);
    // textDocumentSync = Full: the last content change replaces everything.
    const auto& changes = params.at("contentChanges");
    if (changes.empty()) return;
    const auto& last = changes.back();
    if (!last.contains("text")) return;
    auto& d = docs_.open_or_update(uri, last.at("text").get<std::string>(), version);
    publish_diagnostics(d);
}

void server::on_did_save(const json& params) {
    std::string uri = params.at("textDocument").at("uri").get<std::string>();
    const doc_state* d = docs_.find(uri);
    if (!d) return;
    write_cc_to_disk(*d);
}

void server::on_did_close(const json& params) {
    std::string uri = params.at("textDocument").at("uri").get<std::string>();
    docs_.close(uri);
    // Clear diagnostics for the closed URI.
    write_message(out_, make_notification("textDocument/publishDiagnostics",
                                          {{"uri", uri}, {"diagnostics", json::array()}}));
}

void server::publish_diagnostics(const doc_state& d) {
    write_message(out_, make_notification("textDocument/publishDiagnostics",
                                          {{"uri", d.uri},
                                           {"diagnostics", build_diagnostics_payload(d)}}));
}

void server::write_cc_to_disk(const doc_state& d) {
    auto path = uri_to_path(d.uri);
    if (!path) return;
    std::string cc_path = derive_cc_path(*path);
    std::ofstream out(cc_path, std::ios::binary);
    if (!out) {
        log() << "weasel-lsp: cannot write " << cc_path << "\n";
        return;
    }
    try {
        weasel::compiler::transpile(d.text, out);
    } catch (const std::exception& e) {
        log() << "weasel-lsp: transpile failed on save for " << cc_path << ": "
              << e.what() << "\n";
    }
}

} // namespace weasel::lsp
