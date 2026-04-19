#pragma once
#include <string>
#include <variant>
#include <vector>
#include <utility>

namespace weasel {

struct node;
using node_list = std::vector<node>;   // vector<incomplete T> is valid in C++17
using attrs_t   = std::vector<std::pair<std::string, std::string>>;

struct text_node {
    std::string content;
};

// Emits html verbatim — caller guarantees the content is safe HTML.
struct raw_node {
    std::string html;
};

struct element_node {
    std::string tag;
    attrs_t     attrs;
    node_list   children;
};

struct fragment_node {
    node_list children;
};

// All constituent types are complete — std::variant requires this.
struct node : std::variant<std::monostate, text_node, raw_node, element_node, fragment_node> {
    using variant::variant;
};

} // namespace weasel
