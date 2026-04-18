#include "weasel/renderer.hpp"
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
            default:   out << c;
        }
    }
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
                out << ' ' << k << "=\"";
                escape_attr(v, out);
                out << '"';
            }
            if (void_elements.count(e.tag)) {
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

void render(const node& n, std::ostream& out) {
    do_render(n, out);
}

std::string render_to_string(const node& n) {
    std::ostringstream oss;
    do_render(n, oss);
    return oss.str();
}

} // namespace weasel
