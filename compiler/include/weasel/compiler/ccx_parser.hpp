#pragma once
#include "weasel/compiler/source.hpp"
#include <functional>
#include <ostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace weasel::compiler {

class scanner;

struct ccx_attr {
    std::string name;
    std::string value_cpp;
    bool is_literal = false;
    bool has_value = true;
};

struct ccx_node {
    enum class kind { element, text, expr_child, if_chain, for_loop, while_loop };
    kind k = kind::text;

    size_t source_line = 0;  // 1-based .weasel line; 0 = not recorded

    std::string tag_name;
    bool is_component = false;
    std::vector<ccx_attr> attrs;

    std::string text_content;
    std::string expr_text;
    std::string head_cpp;

    struct if_branch {
        std::string cond_cpp;
        std::vector<ccx_node> body;
        bool is_else = false;
        size_t source_line = 0;
    };
    std::vector<if_branch> branches;

    std::vector<ccx_node> children;
};

// Tells the capture callback which delimiter the caller has already consumed.
// paren: caller advanced past '(' (if/for/while head), callback reads until matching ')'.
// brace: caller advanced past '{' (attr value or child expr), callback reads until matching '}'.
enum class capture_kind { paren, brace };

using brace_capture_fn = std::function<void(std::ostream& out, capture_kind kind)>;

// buf may be null; when provided, source_line fields in the returned tree are populated.
ccx_node parse_element(scanner& s,
                       const std::unordered_set<std::string>& components,
                       const brace_capture_fn& capture,
                       const source_buffer* buf = nullptr);

} // namespace weasel::compiler
