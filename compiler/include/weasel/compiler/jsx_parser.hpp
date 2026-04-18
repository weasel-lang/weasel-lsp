#pragma once
#include <functional>
#include <ostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace weasel::compiler {

class scanner;

struct jsx_attr {
    std::string name;
    std::string value_cpp;
    bool is_literal = false;
    bool has_value = true;
};

struct jsx_node {
    enum class kind { element, text, expr_child, if_chain, for_loop, while_loop };
    kind k = kind::text;

    std::string tag_name;
    bool is_component = false;
    std::vector<jsx_attr> attrs;

    std::string text_content;
    std::string expr_text;
    std::string head_cpp;

    struct if_branch {
        std::string cond_cpp;
        std::vector<jsx_node> body;
        bool is_else = false;
    };
    std::vector<if_branch> branches;

    std::vector<jsx_node> children;
};

using brace_capture_fn = std::function<void(std::ostream& out)>;

jsx_node parse_element(scanner& s,
                       const std::unordered_set<std::string>& components,
                       const brace_capture_fn& capture);

} // namespace weasel::compiler
