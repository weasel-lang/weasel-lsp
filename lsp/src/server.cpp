#include "weasel/lsp/server.hpp"
#include <unistd.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include "weasel/compiler/transpiler.hpp"
#include "weasel/lsp/features.hpp"

namespace weasel::lsp {

namespace {

std::string derive_cc_path(std::string_view weasel_path) {
    auto dot = weasel_path.find_last_of('.');
    std::string base = (dot == std::string_view::npos) ? std::string(weasel_path) : std::string(weasel_path.substr(0, dot));
    return base + ".cc";
}

std::string derive_cc_uri(std::string_view weasel_uri) {
    auto dot = weasel_uri.find_last_of('.');
    std::string base = (dot == std::string_view::npos) ? std::string(weasel_uri) : std::string(weasel_uri.substr(0, dot));
    return base + ".cc";
}

bool is_weasel_uri(std::string_view uri) {
    constexpr std::string_view suffix = ".weasel";
    return uri.size() >= suffix.size() && uri.compare(uri.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::ostream& log() {
    return std::cerr;
}

std::string find_compile_commands_dir(std::string_view root_path_hint) {
    // Walk up from hint looking for compile_commands.json. Prefer the
    // conventional location: <root>/build.
    namespace fs = std::filesystem;
    try {
        fs::path p(std::string{root_path_hint});
        if (!root_path_hint.empty()) {
            for (int i = 0; i < 8 && !p.empty(); ++i) {
                if (fs::exists(p / "build" / "compile_commands.json"))
                    return (p / "build").string();
                if (fs::exists(p / "compile_commands.json"))
                    return p.string();
                if (p == p.parent_path())
                    break;
                p = p.parent_path();
            }
        }
    } catch (...) {
    }
    return {};
}

// Recursively replace every occurrence of URI `from` — whether a string value
// or an object key (WorkspaceEdit.changes) — with `to`.
void remap_uris_recursive(json& j, const std::string& from, const std::string& to) {
    if (j.is_string()) {
        if (j.get<std::string>() == from)
            j = to;
    } else if (j.is_object()) {
        if (j.contains(from)) {
            j[to] = std::move(j[from]);
            j.erase(from);
        }
        for (auto& [k, v] : j.items())
            remap_uris_recursive(v, from, to);
    } else if (j.is_array()) {
        for (auto& el : j)
            remap_uris_recursive(el, from, to);
    }
}

}  // namespace

server::server(std::istream& in, std::ostream& out) : in_(in), out_(out) {}

int server::run() {
    while (!shutdown_requested_) {
        auto msg = read_message(in_);
        if (!msg)
            break;
        try {
            handle_message(*msg);
        } catch (const std::exception& e) {
            log() << "weasel-lsp: exception in handler: " << e.what() << "\n";
        }
    }
    if (clangd_)
        clangd_->shutdown();
    return 0;
}

void server::write_to_editor(const json& msg) {
    std::lock_guard<std::mutex> lk(out_mutex_);
    write_message(out_, msg);
}

void server::handle_message(const json& msg) {
    if (!msg.contains("method"))
        return;  // responses to server-initiated reqs
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
        write_to_editor(make_response(id, on_initialize(params)));
        return;
    }
    if (method == "shutdown") {
        shutdown_requested_ = true;
        write_to_editor(make_response(id, nullptr));
        return;
    }
    // $/cancelRequest: relay to clangd and acknowledge with null result.
    if (method == "$/cancelRequest") {
        if (clangd_)
            clangd_->send_notification("$/cancelRequest", params);
        write_to_editor(make_response(id, nullptr));
        return;
    }
    // Try clangd proxy for all textDocument (and future workspace) methods.
    if (forward_request_to_clangd(id, method, params))
        return;
    // Native fallbacks used when clangd is absent or method needs local handling.
    if (method == "textDocument/definition") {
        write_to_editor(make_response(id, on_definition(params)));
        return;
    }
    if (method == "textDocument/completion") {
        write_to_editor(make_response(id, on_completion(params)));
        return;
    }
    write_to_editor(make_error_response(id, -32601, "method not found"));
}

void server::handle_notification(const std::string& method, const json& params) {
    if (method == "initialized") {
        initialized_ = true;
        return;
    }
    if (method == "exit") {
        shutdown_requested_ = true;
        return;
    }
    if (method == "textDocument/didOpen") {
        on_did_open(params);
        return;
    }
    if (method == "textDocument/didChange") {
        on_did_change(params);
        return;
    }
    if (method == "textDocument/didSave") {
        on_did_save(params);
        return;
    }
    if (method == "textDocument/didClose") {
        on_did_close(params);
        return;
    }
    if (method == "$/cancelRequest") {
        if (clangd_)
            clangd_->send_notification(method, params);
        return;
    }
    // Silently drop other notifications (workspace/didChangeConfiguration, etc.).
}

json server::on_initialize(const json& params) {
    // Try to spawn clangd. Use rootUri / rootPath to locate compile_commands.json.
    // Set WEASEL_LSP_NO_CLANGD=1 to skip (useful for tests and v0.5 fallback).
    std::string root_hint;
    if (params.contains("rootUri") && params.at("rootUri").is_string()) {
        if (auto p = uri_to_path(params.at("rootUri").get<std::string>()))
            root_hint = *p;
    }
    if (root_hint.empty() && params.contains("rootPath") && params.at("rootPath").is_string()) {
        root_hint = params.at("rootPath").get<std::string>();
    }
    const char* no_clangd = std::getenv("WEASEL_LSP_NO_CLANGD");
    if (!no_clangd || std::string(no_clangd) != "1") {
        std::string ccd = find_compile_commands_dir(root_hint);
        clangd_ = clangd_proxy::spawn(ccd);
    }
    if (clangd_) {
        log() << "weasel-lsp: clangd spawned\n";
        clangd_->set_notification_handler([this](const std::string& method, const json& p) { on_clangd_notification(method, p); });
        // Initialize clangd in the background. We don't wait for the response
        // before returning our own initialize reply — editor and clangd init
        // in parallel is fine; any didOpen we forward before clangd is ready
        // will be queued on clangd's stdin pipe.
        json init_params = {
            {"processId", ::getpid()},
            {"rootUri", params.value("rootUri", json(nullptr))},
            {"capabilities", json::object()},
        };
        clangd_->send_request("initialize", init_params, [this](const json&, const json&) {
            if (!clangd_)
                return;
            clangd_->send_notification("initialized", json::object());
            // Flush any didOpen/didChange notifications that arrived while
            // clangd was still initializing; now that initialized is sent,
            // clangd is ready to accept them.
            clangd_initialized_ = true;
            for (const auto& pending_uri : clangd_pending_open_uris_) {
                const doc_state* pd = docs_.find(pending_uri);
                if (pd)
                    forward_to_clangd_open(*pd);
            }
            clangd_pending_open_uris_.clear();
        });
    }

    json capabilities = {
        {"textDocumentSync", 1},
        {"definitionProvider", true},
        {"completionProvider",
         {
             {"triggerCharacters", json::array({"<", "/", " ", ".", "::"})},
         }},
        {"hoverProvider", clangd_ != nullptr},
        {"positionEncoding", "utf-8"},
    };
    if (clangd_) {
        capabilities["signatureHelpProvider"] = {
            {"triggerCharacters", json::array({"(", ","})},
        };
        capabilities["referencesProvider"] = true;
        capabilities["documentHighlightProvider"] = true;
        capabilities["documentSymbolProvider"] = true;
        capabilities["codeActionProvider"] = true;
        capabilities["renameProvider"] = true;
        capabilities["inlayHintProvider"] = true;
        capabilities["typeDefinitionProvider"] = true;
        capabilities["implementationProvider"] = true;
    }
    return {
        {"capabilities", capabilities},
        {"serverInfo", {{"name", "weasel-lsp"}, {"version", "1.0.0"}}},
    };
}

json server::on_definition(const json& params) {
    std::string uri = params.at("textDocument").at("uri").get<std::string>();
    int line = params.at("position").at("line").get<int>();
    int character = params.at("position").at("character").get<int>();
    const doc_state* d = docs_.find(uri);
    if (!d)
        return nullptr;
    return build_definition(*d, line, character);
}

json server::on_completion(const json& params) {
    std::string uri = params.at("textDocument").at("uri").get<std::string>();
    int line = params.at("position").at("line").get<int>();
    int character = params.at("position").at("character").get<int>();
    const doc_state* d = docs_.find(uri);
    if (!d)
        return {{"isIncomplete", false}, {"items", json::array()}};
    return build_completion(*d, line, character);
}

void server::on_did_open(const json& params) {
    const auto& td = params.at("textDocument");
    auto& d = docs_.open_or_update(td.at("uri").get<std::string>(), td.at("text").get<std::string>(), td.value("version", 0));
    publish_diagnostics(d);
    forward_to_clangd_open(d);
}

void server::on_did_change(const json& params) {
    std::string uri = params.at("textDocument").at("uri").get<std::string>();
    int version = params.at("textDocument").value("version", 0);
    const auto& changes = params.at("contentChanges");
    if (changes.empty())
        return;
    const auto& last = changes.back();
    if (!last.contains("text"))
        return;
    auto& d = docs_.open_or_update(uri, last.at("text").get<std::string>(), version);
    publish_diagnostics(d);
    forward_to_clangd_change(d);
}

void server::on_did_save(const json& params) {
    std::string uri = params.at("textDocument").at("uri").get<std::string>();
    const doc_state* d = docs_.find(uri);
    if (!d)
        return;
    write_cc_to_disk(*d);
    // Notify clangd so it can re-read if it watches disk (but since we pipe
    // content via didChange, didSave is mostly a hint).
    if (clangd_ && is_weasel_uri(uri)) {
        std::string cc_uri = derive_cc_uri(uri);
        clangd_->send_notification("textDocument/didSave", {
                                                               {"textDocument", {{"uri", cc_uri}}},
                                                               {"text", d->cc_text},
                                                           });
    }
}

void server::on_did_close(const json& params) {
    std::string uri = params.at("textDocument").at("uri").get<std::string>();
    const doc_state* d = docs_.find(uri);
    if (d)
        forward_to_clangd_close(*d);
    docs_.close(uri);
    write_to_editor(make_notification("textDocument/publishDiagnostics", {{"uri", uri}, {"diagnostics", json::array()}}));
}

void server::publish_diagnostics(const doc_state& d) {
    write_to_editor(make_notification("textDocument/publishDiagnostics", {{"uri", d.uri}, {"diagnostics", build_diagnostics_payload(d)}}));
}

void server::write_cc_to_disk(const doc_state& d) {
    auto path = uri_to_path(d.uri);
    if (!path)
        return;
    std::string cc_path = derive_cc_path(*path);
    std::ofstream out(cc_path, std::ios::binary);
    if (!out) {
        log() << "weasel-lsp: cannot write " << cc_path << "\n";
        return;
    }
    out << d.cc_text;
}

// -------------------- clangd proxy integration --------------------

void server::forward_to_clangd_open(const doc_state& d) {
    if (!clangd_ || !is_weasel_uri(d.uri))
        return;
    if (!clangd_initialized_) {
        // Hold until "initialized" has been sent to clangd.
        clangd_pending_open_uris_.push_back(d.uri);
        return;
    }
    std::string cc_uri = derive_cc_uri(d.uri);
    cc_to_weasel_uri_[cc_uri] = d.uri;
    clangd_->send_notification("textDocument/didOpen", {
                                                           {"textDocument",
                                                            {
                                                                {"uri", cc_uri},
                                                                {"languageId", "cpp"},
                                                                {"version", d.version},
                                                                {"text", d.cc_text},
                                                            }},
                                                       });
}

void server::forward_to_clangd_change(const doc_state& d) {
    if (!clangd_ || !is_weasel_uri(d.uri))
        return;
    std::string cc_uri = derive_cc_uri(d.uri);
    cc_to_weasel_uri_[cc_uri] = d.uri;
    clangd_->send_notification("textDocument/didChange", {
                                                             {"textDocument", {{"uri", cc_uri}, {"version", d.version}}},
                                                             {"contentChanges", json::array({
                                                                                    json::object({{"text", d.cc_text}}),
                                                                                })},
                                                         });
}

void server::forward_to_clangd_close(const doc_state& d) {
    if (!clangd_ || !is_weasel_uri(d.uri))
        return;
    std::string cc_uri = derive_cc_uri(d.uri);
    cc_to_weasel_uri_.erase(cc_uri);
    clangd_->send_notification("textDocument/didClose", {
                                                            {"textDocument", {{"uri", cc_uri}}},
                                                        });
}

void server::on_clangd_notification(const std::string& method, const json& params) {
    if (method != "textDocument/publishDiagnostics") {
        // window/logMessage, window/showMessage — relay selectively.
        if (method == "window/logMessage" || method == "window/showMessage") {
            write_to_editor(make_notification(method, params));
        }
        return;
    }
    std::string cc_uri = params.value("uri", "");
    auto it = cc_to_weasel_uri_.find(cc_uri);
    if (it == cc_to_weasel_uri_.end())
        return;  // not a file we manage
    std::string weasel_uri = it->second;

    const doc_state* d = docs_.find(weasel_uri);
    if (!d)
        return;

    // Start with our own parse diagnostics, then append clangd ones remapped.
    json remapped = build_diagnostics_payload(*d);
    if (params.contains("diagnostics") && params.at("diagnostics").is_array()) {
        for (const auto& diag : params.at("diagnostics")) {
            json copy = diag;
            if (!copy.contains("range"))
                continue;
            // LSP line is 0-based; line_map is 1-based. Convert both boundaries.
            int start_line_0 = copy.at("range").at("start").value("line", 0);
            int end_line_0 = copy.at("range").at("end").value("line", 0);
            auto start_r = weasel::compiler::cc_line_to_weasel(d->line_map, start_line_0 + 1);
            auto end_r = weasel::compiler::cc_line_to_weasel(d->line_map, end_line_0 + 1);
            if (!start_r.span)
                continue;  // diag past EOF, skip
            bool in_ccx = start_r.span->kind == weasel::compiler::span_kind::ccx_region;
            int w_start_line = static_cast<int>(start_r.weasel_line) - 1;
            int w_end_line = end_r.span ? static_cast<int>(end_r.weasel_line) - 1 : w_start_line;
            if (in_ccx) {
                // CCX region: column info from clangd refers to emitted .cc,
                // not meaningful in the .weasel. Collapse to a line range.
                copy["range"] = {
                    {"start", {{"line", w_start_line}, {"character", 0}}},
                    {"end", {{"line", w_end_line}, {"character", 10000}}},
                };
                if (copy.contains("message") && copy.at("message").is_string()) {
                    std::string m = copy.at("message").get<std::string>();
                    copy["message"] = "[in CCX] " + m;
                }
            } else {
                copy["range"] = {
                    {"start", {{"line", w_start_line}, {"character", copy.at("range").at("start").value("character", 0)}}},
                    {"end", {{"line", w_end_line}, {"character", copy.at("range").at("end").value("character", 0)}}},
                };
            }
            remapped.push_back(std::move(copy));
        }
    }
    write_to_editor(make_notification("textDocument/publishDiagnostics", {{"uri", weasel_uri}, {"diagnostics", remapped}}));
}

bool server::forward_request_to_clangd(const json& id, const std::string& method, const json& params) {
    if (!clangd_)
        return false;
    if (!params.contains("textDocument"))
        return false;
    std::string uri = params.at("textDocument").at("uri").get<std::string>();
    if (!is_weasel_uri(uri))
        return false;

    const doc_state* d = docs_.find(uri);
    if (!d)
        return false;

    // Position-bearing requests may need CCX-aware routing.
    if (params.contains("position")) {
        int line = params.at("position").at("line").get<int>();
        int character = params.at("position").at("character").get<int>();
        size_t offset = offset_from_lsp_position(d->buffer, line, character);
        bool in_ccx = d->position_in_ccx(offset);

        if (method == "textDocument/definition" && !in_ccx) {
            json local = build_definition(*d, line, character);
            if (!local.is_null()) {
                write_to_editor(make_response(id, local));
                return true;
            }
            // fall through to clangd
        } else if (method == "textDocument/definition" && in_ccx) {
            return false;  // component go-to only; native handler covers it
        } else if (method == "textDocument/completion" && in_ccx) {
            if (!d->position_in_ccx_expression(offset))
                return false;  // markup position; Weasel handles it
            auto remapped_char = remap_ccx_completion_column(*d, line, character);
            if (!remapped_char)
                return false;
            std::string cc_uri = derive_cc_uri(uri);
            json translated = params;
            translated["textDocument"]["uri"] = cc_uri;
            translated["position"]["character"] = *remapped_char;
            json cap_id = id;
            clangd_->send_request(method, translated, [this, cap_id](const json& result, const json& err) {
                write_to_editor(make_response(cap_id, err.is_null() ? result : json(nullptr)));
            });
            return true;
        } else if ((method == "textDocument/hover" || method == "textDocument/signatureHelp") && in_ccx) {
            auto remapped_char = remap_ccx_hover_column(*d, line, character);
            if (!remapped_char)
                return false;
            std::string cc_uri = derive_cc_uri(uri);
            json translated = params;
            translated["textDocument"]["uri"] = cc_uri;
            translated["position"]["character"] = *remapped_char;
            json cap_id = id;
            clangd_->send_request(method, translated, [this, cap_id](const json& result, const json& err) {
                write_to_editor(make_response(cap_id, err.is_null() ? result : json(nullptr)));
            });
            return true;
        }
    }

    // Generic passthrough: translate URI, forward, remap URIs in response.
    // Covers completion/hover/references/rename/codeAction/inlayHints/etc.
    std::string cc_uri = derive_cc_uri(uri);
    json translated = params;
    translated["textDocument"]["uri"] = cc_uri;

    json cap_id = id;
    clangd_->send_request(method, translated, [this, cap_id, uri, cc_uri](const json& result, const json& err) {
        if (!err.is_null()) {
            write_to_editor(make_response(cap_id, nullptr));
            return;
        }
        json out = result;
        remap_uris_recursive(out, cc_uri, uri);
        write_to_editor(make_response(cap_id, out));
    });
    return true;
}

}  // namespace weasel::lsp
