#include "weasel/compiler/ccx_parser.hpp"
#include "weasel/compiler/diagnostic.hpp"
#include "weasel/compiler/scanner.hpp"
#include <sstream>

namespace weasel::compiler {
namespace {

[[noreturn]] void fail(scanner& s, const std::string& msg) {
    diagnostic d;
    d.sev = severity::error;
    d.span = {s.pos(), s.pos()};
    std::ostringstream o;
    o << "weasel parse error: " << msg;
    d.message = o.str();
    d.code = "weasel.parse";
    throw parse_error(std::move(d));
}

void skip_ws(scanner& s) { s.read_whitespace(); }

std::string read_tag_name(scanner& s) {
    auto id = std::string(s.read_identifier());
    if (id.empty()) fail(s, "expected tag name");
    while (s.peek() == '-' && scanner::is_ident_start(s.peek(1))) {
        id += '-';
        s.advance();
        id += std::string(s.read_identifier());
    }
    return id;
}

std::vector<ccx_node> parse_children(scanner& s,
                                     const std::unordered_set<std::string>& comps,
                                     const brace_capture_fn& cap,
                                     bool terminator_is_rbrace,
                                     std::string_view element_name);

ccx_node parse_block_body(scanner& s,
                          const std::unordered_set<std::string>& comps,
                          const brace_capture_fn& cap,
                          std::vector<ccx_node>& out_children) {
    // Precondition: we're about to consume '{'
    skip_ws(s);
    if (s.peek() != '{') fail(s, "expected '{' for block body");
    s.advance();
    out_children = parse_children(s, comps, cap, /*terminator_is_rbrace=*/true, /*element_name=*/{});
    if (s.peek() != '}') fail(s, "expected '}' closing block body");
    s.advance();
    return {};
}

ccx_node parse_if_child(scanner& s,
                        const std::unordered_set<std::string>& comps,
                        const brace_capture_fn& cap) {
    ccx_node n;
    n.k = ccx_node::kind::if_chain;
    // Keyword 'if' already consumed by caller.
    auto parse_cond_and_body = [&](ccx_node::if_branch& br) {
        skip_ws(s);
        if (s.peek() != '(') fail(s, "expected '(' after 'if'");
        s.advance();
        std::ostringstream cond;
        cap(cond);
        br.cond_cpp = cond.str();
        std::vector<ccx_node> body;
        parse_block_body(s, comps, cap, body);
        br.body = std::move(body);
    };

    {
        ccx_node::if_branch first;
        parse_cond_and_body(first);
        n.branches.push_back(std::move(first));
    }

    while (true) {
        size_t save = s.pos();
        skip_ws(s);
        if (!scanner::is_ident_start(s.peek())) { s.set_pos(save); break; }
        auto id = s.read_identifier();
        if (id != "else") { s.set_pos(save); break; }
        skip_ws(s);
        if (scanner::is_ident_start(s.peek())) {
            auto id2 = s.read_identifier();
            if (id2 != "if") fail(s, "expected 'if' or '{' after 'else'");
            ccx_node::if_branch br;
            parse_cond_and_body(br);
            n.branches.push_back(std::move(br));
            continue;
        }
        if (s.peek() != '{') fail(s, "expected '{' or 'if' after 'else'");
        ccx_node::if_branch else_br;
        else_br.is_else = true;
        std::vector<ccx_node> body;
        parse_block_body(s, comps, cap, body);
        else_br.body = std::move(body);
        n.branches.push_back(std::move(else_br));
        break;
    }
    return n;
}

ccx_node parse_for_child(scanner& s,
                         const std::unordered_set<std::string>& comps,
                         const brace_capture_fn& cap) {
    ccx_node n;
    n.k = ccx_node::kind::for_loop;
    skip_ws(s);
    if (s.peek() != '(') fail(s, "expected '(' after 'for'");
    s.advance();
    std::ostringstream head;
    cap(head);
    n.head_cpp = head.str();
    std::vector<ccx_node> body;
    parse_block_body(s, comps, cap, body);
    n.children = std::move(body);
    return n;
}

ccx_node parse_while_child(scanner& s,
                           const std::unordered_set<std::string>& comps,
                           const brace_capture_fn& cap) {
    ccx_node n;
    n.k = ccx_node::kind::while_loop;
    skip_ws(s);
    if (s.peek() != '(') fail(s, "expected '(' after 'while'");
    s.advance();
    std::ostringstream head;
    cap(head);
    n.head_cpp = head.str();
    std::vector<ccx_node> body;
    parse_block_body(s, comps, cap, body);
    n.children = std::move(body);
    return n;
}

ccx_node parse_brace_child(scanner& s,
                           const std::unordered_set<std::string>& comps,
                           const brace_capture_fn& cap) {
    // Precondition: s.peek() == '{'
    s.advance();
    size_t after_open = s.pos();
    skip_ws(s);
    if (scanner::is_ident_start(s.peek())) {
        auto id = s.read_identifier();
        ccx_node result;
        bool handled = false;
        if (id == "if")         { result = parse_if_child(s, comps, cap); handled = true; }
        else if (id == "for")   { result = parse_for_child(s, comps, cap); handled = true; }
        else if (id == "while") { result = parse_while_child(s, comps, cap); handled = true; }
        if (handled) {
            skip_ws(s);
            if (s.peek() != '}') fail(s, "expected '}' after control-flow block");
            s.advance();
            return result;
        }
        s.set_pos(after_open);
    }
    std::ostringstream oss;
    cap(oss);
    ccx_node n;
    n.k = ccx_node::kind::expr_child;
    n.expr_text = oss.str();
    return n;
}

ccx_attr parse_attr(scanner& s, const brace_capture_fn& cap) {
    ccx_attr a;
    a.name = std::string(s.read_identifier());
    if (a.name.empty()) fail(s, "expected attribute name");
    skip_ws(s);
    if (s.peek() != '=') {
        a.has_value = false;
        return a;
    }
    s.advance(); // =
    skip_ws(s);
    if (s.peek() == '"') {
        a.value_cpp = std::string(s.read_string_literal());
        a.is_literal = true;
    } else if (s.peek() == '{') {
        s.advance(); // past {
        std::ostringstream oss;
        cap(oss);
        a.value_cpp = oss.str();
        a.is_literal = false;
    } else {
        fail(s, "expected '\"' or '{' after '=' in attribute");
    }
    return a;
}

// JSX-style text normalization:
// - If the run begins with whitespace + newline, drop that whole leading run.
// - If it ends with newline + whitespace, drop that whole trailing run.
// - Within the remaining content, split on newlines, left-trim each line,
//   drop empty lines, join with a single space.
// - If the run has no leading/trailing newline, it is kept verbatim.
std::string normalize_text(const std::string& raw) {
    bool all_ws = true;
    for (char c : raw) if (!scanner::is_whitespace(c)) { all_ws = false; break; }
    if (all_ws) return {};

    size_t i = 0;
    bool leading_newline = false;
    while (i < raw.size() && scanner::is_whitespace(raw[i])) {
        if (raw[i] == '\n' || raw[i] == '\r') leading_newline = true;
        i++;
    }
    size_t start = leading_newline ? i : 0;

    size_t j = raw.size();
    bool trailing_newline = false;
    while (j > start && scanner::is_whitespace(raw[j - 1])) {
        if (raw[j - 1] == '\n' || raw[j - 1] == '\r') trailing_newline = true;
        j--;
    }
    size_t end = trailing_newline ? j : raw.size();

    std::string inner = raw.substr(start, end - start);
    if (!leading_newline && !trailing_newline) return inner;

    std::string out;
    size_t p = 0;
    bool first = true;
    while (p <= inner.size()) {
        size_t nl = inner.find('\n', p);
        std::string line = (nl == std::string::npos) ? inner.substr(p) : inner.substr(p, nl - p);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t lt = 0;
        while (lt < line.size() && (line[lt] == ' ' || line[lt] == '\t')) lt++;
        std::string trimmed = line.substr(lt);
        if (!trimmed.empty()) {
            if (!first) out += ' ';
            out += trimmed;
            first = false;
        }
        if (nl == std::string::npos) break;
        p = nl + 1;
    }
    return out;
}

std::string read_text_run(scanner& s, bool stop_at_rbrace) {
    std::string out;
    while (!s.eof()) {
        char c = s.peek();
        if (c == '<' || c == '{') break;
        if (c == '}' && stop_at_rbrace) break;
        out += c;
        s.advance();
    }
    return out;
}

std::vector<ccx_node> parse_children(scanner& s,
                                     const std::unordered_set<std::string>& comps,
                                     const brace_capture_fn& cap,
                                     bool terminator_is_rbrace,
                                     std::string_view element_name) {
    std::vector<ccx_node> children;
    while (!s.eof()) {
        char c = s.peek();
        if (c == '<') {
            if (s.peek(1) == '/') {
                // Close tag — belongs to an element child
                if (terminator_is_rbrace) fail(s, "unexpected closing tag in block body");
                // Consume `</name>`
                s.advance(2);
                skip_ws(s);
                std::string name = read_tag_name(s);
                skip_ws(s);
                if (s.peek() != '>') fail(s, "expected '>' in closing tag");
                s.advance();
                if (name != element_name) fail(s, "mismatched closing tag");
                return children;
            }
            // Nested element
            children.push_back(parse_element(s, comps, cap));
        } else if (c == '{') {
            children.push_back(parse_brace_child(s, comps, cap));
        } else if (c == '}' && terminator_is_rbrace) {
            return children;
        } else {
            std::string text = read_text_run(s, terminator_is_rbrace);
            std::string normalized = normalize_text(text);
            if (!normalized.empty()) {
                ccx_node n;
                n.k = ccx_node::kind::text;
                n.text_content = normalized;
                children.push_back(std::move(n));
            }
        }
    }
    if (terminator_is_rbrace) return children;
    fail(s, "unexpected end of input inside element children");
}

} // namespace

ccx_node parse_element(scanner& s,
                       const std::unordered_set<std::string>& comps,
                       const brace_capture_fn& cap) {
    if (s.peek() != '<') fail(s, "expected '<' at element start");
    s.advance();
    skip_ws(s);
    ccx_node n;
    n.k = ccx_node::kind::element;
    n.tag_name = read_tag_name(s);
    n.is_component = comps.count(n.tag_name) > 0;
    // Parse attributes
    while (true) {
        skip_ws(s);
        char c = s.peek();
        if (c == '/' || c == '>') break;
        if (c == '\0') fail(s, "unexpected end of input inside tag");
        n.attrs.push_back(parse_attr(s, cap));
    }
    if (s.peek() == '/') {
        s.advance();
        if (s.peek() != '>') fail(s, "expected '>' after '/' in self-closing tag");
        s.advance();
        return n;
    }
    if (s.peek() != '>') fail(s, "expected '>' at end of opening tag");
    s.advance();
    n.children = parse_children(s, comps, cap, /*terminator_is_rbrace=*/false, n.tag_name);
    return n;
}

} // namespace weasel::compiler
