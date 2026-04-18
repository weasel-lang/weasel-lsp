#pragma once
#include <iosfwd>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace weasel::lsp {

using json = nlohmann::json;

// Read one LSP JSON-RPC message from an input stream. Handles the Content-Length
// framing. Returns nullopt on EOF or malformed input.
std::optional<json> read_message(std::istream& in);

// Write one LSP JSON-RPC message to an output stream (with framing) and flush.
void write_message(std::ostream& out, const json& msg);

// Helper: build a JSON-RPC response object.
json make_response(const json& id, json result);
json make_error_response(const json& id, int code, std::string_view message);

// Helper: build a server-initiated notification.
json make_notification(std::string_view method, json params);

} // namespace weasel::lsp
