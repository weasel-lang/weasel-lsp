#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "weasel/compiler/boundary.hpp"
#include "weasel/compiler/diagnostic.hpp"
#include "weasel/compiler/source.hpp"
#include "weasel/compiler/transpiler.hpp"

namespace weasel::lsp {

struct doc_state {
    std::string uri;
    std::string text;
    int version = 0;

    weasel::compiler::source_buffer buffer;
    std::vector<weasel::compiler::component_info> components;
    std::vector<weasel::compiler::ccx_span> ccx_spans;

    // Parse diagnostic if transpile failed. Empty if it succeeded.
    std::vector<weasel::compiler::diagnostic> diagnostics;

    // Transpiled .cc text and the line map from the last successful transpile.
    // If the transpile failed the cc_text may be partial; line_map corresponds
    // to whatever was produced before the parse_error.
    std::string cc_text;
    std::vector<weasel::compiler::line_span> line_map;

    // Byte offset <-> (line, column) mapping is via `buffer`.
    bool position_in_ccx(size_t offset) const;
    // Returns true if offset is inside a {…} C++ expression within any CCX span
    // (attribute value or child expression), rather than in a markup context.
    bool position_in_ccx_expression(size_t offset) const;
};

class document_store {
   public:
    // Open/update a document. Reruns transpile-scan and refreshes caches.
    doc_state& open_or_update(std::string uri, std::string text, int version);
    void close(const std::string& uri);
    const doc_state* find(const std::string& uri) const;
    doc_state* find(const std::string& uri);

   private:
    std::unordered_map<std::string, doc_state> docs_;

    // Recompute components, spans, and diagnostics for `d` based on d.text.
    static void refresh(doc_state& d);
};

// Utility: convert file:// URI to filesystem path (minimal; assumes UTF-8 ASCII).
std::optional<std::string> uri_to_path(std::string_view uri);

// Utility: convert filesystem path to file:// URI.
std::string path_to_uri(std::string_view path);

// Byte-offset <-> UTF-8 column conversion helpers via source_buffer are exposed
// by `compiler/source.hpp::source_buffer::position_of`. The LSP publishes
// positions as {line: 0-based, character: 0-based}. `position_of` returns
// 1-based line; use the helper below to convert.

struct lsp_position {
    int line;       // 0-based
    int character;  // 0-based
};

lsp_position to_lsp_position(const weasel::compiler::source_buffer& buf, size_t offset);

// Given an LSP position, find the byte offset in the buffer. Clamps to EOF.
size_t offset_from_lsp_position(const weasel::compiler::source_buffer& buf, int line, int character);

}  // namespace weasel::lsp
