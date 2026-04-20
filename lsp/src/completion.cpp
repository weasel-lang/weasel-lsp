#include "weasel/lsp/features.hpp"
#include <array>
#include <string_view>

namespace weasel::lsp {

namespace {

// Minimal HTML5 element list (non-exhaustive but enough for v0.5).
constexpr auto html_tags = std::to_array<std::string_view>({
    "a", "abbr", "address", "area", "article", "aside", "audio", "b", "base",
    "bdi", "bdo", "blockquote", "body", "br", "button", "canvas", "caption",
    "cite", "code", "col", "colgroup", "data", "datalist", "dd", "del",
    "details", "dfn", "dialog", "div", "dl", "dt", "em", "embed", "fieldset",
    "figcaption", "figure", "footer", "form", "h1", "h2", "h3", "h4", "h5",
    "h6", "head", "header", "hr", "html", "i", "iframe", "img", "input",
    "ins", "kbd", "label", "legend", "li", "link", "main", "map", "mark",
    "menu", "meta", "meter", "nav", "noscript", "object", "ol", "optgroup",
    "option", "output", "p", "param", "picture", "pre", "progress", "q", "rp",
    "rt", "ruby", "s", "samp", "script", "section", "select", "small",
    "source", "span", "strong", "style", "sub", "summary", "sup", "table",
    "tbody", "td", "template", "textarea", "tfoot", "th", "thead", "time",
    "title", "tr", "track", "u", "ul", "var", "video", "wbr",
});

constexpr int kKindText = 1;
constexpr int kKindClass = 7;
constexpr int kKindKeyword = 14;

} // namespace

json build_completion(const doc_state& d, int line, int character) {
    json items = json::array();

    size_t offset = offset_from_lsp_position(d.buffer, line, character);

    if (!d.position_in_ccx(offset)) {
        // v0.5: no C++ completion of our own; clangd will handle .cc.
        return {{"isIncomplete", false}, {"items", items}};
    }
    if (d.position_in_ccx_expression(offset)) {
        // Inside a {…} C++ expression: clangd handles this; return nothing.
        return {{"isIncomplete", false}, {"items", items}};
    }

    for (auto tag : html_tags) {
        items.push_back({
            {"label", std::string(tag)},
            {"kind", kKindText},
            {"detail", "HTML element"},
        });
    }
    for (const auto& c : d.components) {
        items.push_back({
            {"label", c.name},
            {"kind", kKindClass},
            {"detail", "weasel component"},
        });
    }
    for (auto kw : {"if", "for", "while", "else"}) {
        items.push_back({
            {"label", kw},
            {"kind", kKindKeyword},
        });
    }

    return {{"isIncomplete", false}, {"items", items}};
}

} // namespace weasel::lsp
