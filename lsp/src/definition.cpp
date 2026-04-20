#include "weasel/compiler/scanner.hpp"
#include "weasel/lsp/features.hpp"

namespace weasel::lsp {

namespace {

// Returns the identifier covering `offset` in `text`, or empty if none.
std::string identifier_at(std::string_view text, size_t offset) {
    if (offset > text.size())
        return {};
    size_t start = offset;
    while (start > 0 && weasel::compiler::scanner::is_ident_cont(text[start - 1])) {
        --start;
    }
    size_t end = offset;
    while (end < text.size() && weasel::compiler::scanner::is_ident_cont(text[end])) {
        ++end;
    }
    if (start == end)
        return {};
    if (!weasel::compiler::scanner::is_ident_start(text[start]))
        return {};
    return std::string(text.substr(start, end - start));
}

}  // namespace

json build_definition(const doc_state& d, int line, int character) {
    size_t offset = offset_from_lsp_position(d.buffer, line, character);
    std::string ident = identifier_at(d.text, offset);
    if (ident.empty())
        return nullptr;

    for (const auto& c : d.components) {
        if (c.name == ident) {
            auto start = to_lsp_position(d.buffer, c.decl_offset);
            int end_character = start.character + static_cast<int>(c.name.size());
            return json{
                {"uri", d.uri},
                {"range",
                 {
                     {"start", {{"line", start.line}, {"character", start.character}}},
                     {"end", {{"line", start.line}, {"character", end_character}}},
                 }},
            };
        }
    }
    return nullptr;
}

}  // namespace weasel::lsp
