#pragma once
#include "weasel/lsp/document_store.hpp"
#include "weasel/lsp/jsonrpc.hpp"
#include <iosfwd>
#include <string>

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

    std::istream& in_;
    std::ostream& out_;
    document_store docs_;
    bool initialized_ = false;
    bool shutdown_requested_ = false;
};

} // namespace weasel::lsp
