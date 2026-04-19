#pragma once
#include "weasel/lsp/clangd_proxy.hpp"
#include "weasel/lsp/document_store.hpp"
#include "weasel/lsp/jsonrpc.hpp"
#include <iosfwd>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace weasel::lsp {

class server {
  public:
    server(std::istream& in, std::ostream& out);
    int run();  // returns process exit code

  private:
    void handle_message(const json& msg);
    void handle_request(const json& id, const std::string& method, const json& params);
    void handle_notification(const std::string& method, const json& params);

    // Request handlers
    json on_initialize(const json& params);
    json on_definition(const json& params);
    json on_completion(const json& params);

    // Notification handlers
    void on_did_open(const json& params);
    void on_did_change(const json& params);
    void on_did_save(const json& params);
    void on_did_close(const json& params);

    // Internal helpers
    void publish_diagnostics(const doc_state& d);
    void write_cc_to_disk(const doc_state& d);

    // Thread-safe write to editor output.
    void write_to_editor(const json& msg);

    // clangd-proxy integration (v1). If `clangd_` is null, v0.5 fallback
    // behavior is used (no C++ intelligence).
    void forward_to_clangd_open(const doc_state& d);
    void forward_to_clangd_change(const doc_state& d);
    void forward_to_clangd_close(const doc_state& d);
    void on_clangd_notification(const std::string& method, const json& params);
    // Forward an editor-originated request to clangd; translates position and
    // remaps response. Returns true if handled (response already written).
    // Returns false if the request should fall through to the v0.5 path (e.g.
    // clangd not available or position is inside a CCX region).
    bool forward_request_to_clangd(const json& id, const std::string& method,
                                   const json& params);

    std::istream& in_;
    std::ostream& out_;
    std::mutex out_mutex_;
    document_store docs_;
    bool initialized_ = false;
    bool shutdown_requested_ = false;

    std::unique_ptr<clangd_proxy> clangd_;
    // Map .cc URI -> .weasel URI, for remapping clangd diagnostics.
    std::unordered_map<std::string, std::string> cc_to_weasel_uri_;
};

} // namespace weasel::lsp
