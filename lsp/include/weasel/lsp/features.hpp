#pragma once
#include "weasel/lsp/document_store.hpp"
#include "weasel/lsp/jsonrpc.hpp"

namespace weasel::lsp {

// Build a CompletionList JSON for the given document + cursor position.
// Returns an empty CompletionList if the cursor is outside CCX.
json build_completion(const doc_state& d, int line, int character);

// Build a Location/null JSON for textDocument/definition.
json build_definition(const doc_state& d, int line, int character);

// Build a list of LSP Diagnostic objects from a doc_state's parse diagnostics.
json build_diagnostics_payload(const doc_state& d);

} // namespace weasel::lsp
