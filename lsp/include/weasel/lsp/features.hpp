#pragma once
#include "weasel/lsp/document_store.hpp"
#include "weasel/lsp/jsonrpc.hpp"
#include <optional>
#include <string_view>

namespace weasel::lsp {

// Build a CompletionList JSON for the given document + cursor position.
// Returns an empty CompletionList if the cursor is outside CCX.
json build_completion(const doc_state& d, int line, int character);

// Build a Location/null JSON for textDocument/definition.
json build_definition(const doc_state& d, int line, int character);

// Build a list of LSP Diagnostic objects from a doc_state's parse diagnostics.
json build_diagnostics_payload(const doc_state& d);

// Return the content of 0-based line `n` from a multi-line string.
std::string_view get_text_line(std::string_view text, int line_0);

// Remap a weasel cursor position inside a CCX region to the corresponding
// column in the generated .cc line, for hover and signature-help requests.
std::optional<int> remap_ccx_hover_column(const doc_state& d,
                                           int weasel_line_0, int weasel_char_0);

// Like remap_ccx_hover_column but handles '.' and '::' trigger characters,
// used for completion requests inside CCX expressions.
std::optional<int> remap_ccx_completion_column(const doc_state& d,
                                                int weasel_line_0, int weasel_char_0);

} // namespace weasel::lsp
