#include "weasel/renderer.hpp"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <ostream>
#include <sstream>
#include <unordered_set>
#include <string_view>

namespace weasel {
namespace {

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

using base_variant = std::variant<std::monostate, text_node, raw_node, element_node, fragment_node>;

const std::unordered_set<std::string_view> void_elements = {
    "area","base","br","col","embed","hr","img","input",
    "link","meta","param","source","track","wbr"
};

void escape_text(std::string_view in, std::ostream& out) {
    for (char c : in) {
        switch (c) {
            case '&': out << "&amp;";  break;
            case '<': out << "&lt;";   break;
            case '>': out << "&gt;";   break;
            default:  out << c;
        }
    }
}

void escape_attr(std::string_view in, std::ostream& out) {
    for (char c : in) {
        switch (c) {
            case '&':  out << "&amp;";  break;
            case '<':  out << "&lt;";   break;
            case '>':  out << "&gt;";   break;
            case '"':  out << "&quot;"; break;
            case '\'': out << "&#39;";  break;
            case '\n': out << "&#10;";  break;
            case '\r': out << "&#13;";  break;
            case '\t': out << "&#9;";   break;
            default:   out << c;
        }
    }
}

bool is_valid_attr_name(std::string_view k) {
    if (k.empty()) return false;
    auto first_ok = [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == ':';
    };
    auto rest_ok = [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9') || c == '_' || c == ':' || c == '.' || c == '-';
    };
    if (!first_ok(k[0])) return false;
    for (size_t i = 1; i < k.size(); ++i)
        if (!rest_ok(k[i])) return false;
    return true;
}

void do_render(const node& n, std::ostream& out) {
    std::visit(overloaded{
        [](std::monostate) {},
        [&](const text_node& t) {
            escape_text(t.content, out);
        },
        [&](const raw_node& r) {
            out << r.html;
        },
        [&](const element_node& e) {
            out << '<' << e.tag;
            for (const auto& [k, v] : e.attrs) {
                assert(is_valid_attr_name(k) && "attribute name must match [A-Za-z_:][A-Za-z0-9_:.-]*");
                out << ' ' << k << "=\"";
                escape_attr(v, out);
                out << '"';
            }
            // Normalize to lowercase for case-insensitive void-element lookup.
            std::string lower_tag(e.tag.size(), '\0');
            std::transform(e.tag.begin(), e.tag.end(), lower_tag.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (void_elements.count(lower_tag)) {
                out << '>';
            } else {
                out << '>';
                for (const auto& c : e.children)
                    do_render(c, out);
                out << "</" << e.tag << '>';
            }
        },
        [&](const fragment_node& f) {
            for (const auto& c : f.children)
                do_render(c, out);
        }
    }, static_cast<const base_variant&>(n));
}

} // anonymous namespace

void render(const node& n, std::ostream& out) noexcept {
    do_render(n, out);
}

std::string render_to_string(const node& n) noexcept {
    std::ostringstream oss;
    do_render(n, oss);
    return oss.str();
}

} // namespace weasel
