#pragma once
#include "weasel/node.hpp"
#include <string_view>

namespace weasel {

inline node text(std::string_view content) {
    return text_node{std::string(content)};
}

inline node raw(std::string_view html) {
    return raw_node{std::string(html)};
}

inline node fragment(node_list children) {
    return fragment_node{std::move(children)};
}

// Precondition: `name` must be lowercase (HTML tag names are case-insensitive;
// void-element detection normalises to lowercase at render time). Attribute
// keys must match [A-Za-z_:][A-Za-z0-9_:.-]* — this is asserted in debug builds.
inline node tag(std::string_view name,
                attrs_t attrs = {},
                node_list children = {}) {
    return element_node{std::string(name), std::move(attrs), std::move(children)};
}

} // namespace weasel
