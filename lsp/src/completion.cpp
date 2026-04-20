#include <array>
#include <optional>
#include <string_view>
#include "weasel/lsp/features.hpp"

namespace weasel::lsp {

std::string_view get_text_line(std::string_view text, int line_0) {
    size_t pos = 0;
    for (int i = 0; i < line_0; ++i) {
        auto nl = text.find('\n', pos);
        if (nl == std::string_view::npos)
            return {};
        pos = nl + 1;
    }
    auto end = text.find('\n', pos);
    if (end == std::string_view::npos)
        end = text.size();
    return text.substr(pos, end - pos);
}

std::optional<int> remap_ccx_hover_column(const doc_state& d, int weasel_line_0, int weasel_char_0) {
    auto cc_line = get_text_line(d.cc_text, weasel_line_0);
    bool has_content = false;
    for (char c : cc_line) {
        if (c != ' ' && c != '\t') {
            has_content = true;
            break;
        }
    }
    if (!has_content)
        return std::nullopt;

    auto weasel_line = get_text_line(d.text, weasel_line_0);
    if (weasel_char_0 < 0 || (size_t)weasel_char_0 >= weasel_line.size())
        return std::nullopt;

    auto is_ident = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; };
    size_t cur = (size_t)weasel_char_0;
    if (!is_ident(weasel_line[cur]))
        return std::nullopt;

    size_t id_start = cur;
    while (id_start > 0 && is_ident(weasel_line[id_start - 1]))
        --id_start;
    size_t id_end = cur + 1;
    while (id_end < weasel_line.size() && is_ident(weasel_line[id_end]))
        ++id_end;

    std::string_view id_text = weasel_line.substr(id_start, id_end - id_start);
    if (id_text.empty())
        return std::nullopt;

    size_t search_from = 0;
    while (search_from < cc_line.size()) {
        auto pos = cc_line.find(id_text, search_from);
        if (pos == std::string_view::npos)
            break;
        bool left_ok = (pos == 0) || !is_ident(cc_line[pos - 1]);
        bool right_ok = (pos + id_text.size() >= cc_line.size()) || !is_ident(cc_line[pos + id_text.size()]);
        if (left_ok && right_ok) {
            size_t offset_in_id = cur - id_start;
            return static_cast<int>(pos + offset_in_id);
        }
        search_from = pos + 1;
    }
    return std::nullopt;
}

std::optional<int> remap_ccx_completion_column(const doc_state& d, int weasel_line_0, int weasel_char_0) {
    auto cc_line = get_text_line(d.cc_text, weasel_line_0);
    bool has_content = false;
    for (char c : cc_line) {
        if (c != ' ' && c != '\t') {
            has_content = true;
            break;
        }
    }
    if (!has_content)
        return std::nullopt;

    auto weasel_line = get_text_line(d.text, weasel_line_0);
    if (weasel_char_0 < 0)
        return std::nullopt;

    auto is_ident = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; };

    size_t cur = std::min((size_t)weasel_char_0, weasel_line.size());

    size_t trigger_start = std::string_view::npos;
    size_t trigger_len = 0;
    if (cur >= 1 && weasel_line[cur - 1] == '.') {
        trigger_start = cur - 1;
        trigger_len = 1;
    } else if (cur >= 2 && weasel_line[cur - 1] == ':' && weasel_line[cur - 2] == ':') {
        trigger_start = cur - 2;
        trigger_len = 2;
    }

    if (trigger_start != std::string_view::npos && trigger_start > 0 && is_ident(weasel_line[trigger_start - 1])) {
        size_t id_end = trigger_start;
        size_t id_start = id_end;
        while (id_start > 0 && is_ident(weasel_line[id_start - 1]))
            --id_start;
        std::string_view id_text = weasel_line.substr(id_start, id_end - id_start);
        if (!id_text.empty()) {
            size_t search_from = 0;
            while (search_from < cc_line.size()) {
                auto pos = cc_line.find(id_text, search_from);
                if (pos == std::string_view::npos)
                    break;
                bool left_ok = (pos == 0) || !is_ident(cc_line[pos - 1]);
                bool right_ok = (pos + id_text.size() >= cc_line.size()) || !is_ident(cc_line[pos + id_text.size()]);
                if (left_ok && right_ok)
                    return static_cast<int>(pos + id_text.size() + trigger_len);
                search_from = pos + 1;
            }
        }
    }

    return remap_ccx_hover_column(d, weasel_line_0, weasel_char_0);
}

namespace {

// Minimal HTML5 element list (non-exhaustive but enough for v0.5).
constexpr auto html_tags = std::to_array<std::string_view>({
    "a",          "abbr",       "address",  "area",    "article", "aside",   "audio",   "b",       "base",     "bdi",      "bdo",
    "blockquote", "body",       "br",       "button",  "canvas",  "caption", "cite",    "code",    "col",      "colgroup", "data",
    "datalist",   "dd",         "del",      "details", "dfn",     "dialog",  "div",     "dl",      "dt",       "em",       "embed",
    "fieldset",   "figcaption", "figure",   "footer",  "form",    "h1",      "h2",      "h3",      "h4",       "h5",       "h6",
    "head",       "header",     "hr",       "html",    "i",       "iframe",  "img",     "input",   "ins",      "kbd",      "label",
    "legend",     "li",         "link",     "main",    "map",     "mark",    "menu",    "meta",    "meter",    "nav",      "noscript",
    "object",     "ol",         "optgroup", "option",  "output",  "p",       "param",   "picture", "pre",      "progress", "q",
    "rp",         "rt",         "ruby",     "s",       "samp",    "script",  "section", "select",  "small",    "source",   "span",
    "strong",     "style",      "sub",      "summary", "sup",     "table",   "tbody",   "td",      "template", "textarea", "tfoot",
    "th",         "thead",      "time",     "title",   "tr",      "track",   "u",       "ul",      "var",      "video",    "wbr",
});

constexpr int kKindText = 1;
constexpr int kKindClass = 7;
constexpr int kKindKeyword = 14;

}  // namespace

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

}  // namespace weasel::lsp
