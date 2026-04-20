#include "weasel/compiler/ccx_parser.hpp"
#include <sstream>
#include "weasel/compiler/diagnostic.hpp"
#include "weasel/compiler/scanner.hpp"

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

void skip_ws(scanner& s) {
    s.read_whitespace();
}

std::string read_tag_name(scanner& s) {
    auto id = std::string(s.read_identifier());
    if (id.empty())
        fail(s, "expected tag name");
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
                                     std::string_view element_name,
                                     const source_buffer* buf);

ccx_node parse_block_body(scanner& s,
                          const std::unordered_set<std::string>& comps,
                          const brace_capture_fn& cap,
                          std::vector<ccx_node>& out_children,
                          const source_buffer* buf) {
    skip_ws(s);
    if (s.peek() != '{')
        fail(s, "expected '{' for block body");
    s.advance();
    out_children = parse_children(s, comps, cap, /*terminator_is_rbrace=*/true, /*element_name=*/{}, buf);
    if (s.peek() != '}')
        fail(s, "expected '}' closing block body");
    s.advance();
    return {};
}

ccx_node parse_if_child(scanner& s,
                        const std::unordered_set<std::string>& comps,
                        const brace_capture_fn& cap,
                        const source_buffer* buf,
                        size_t if_kw_pos) {
    ccx_node n;
    n.k = ccx_node::kind::if_chain;
    n.source_line = buf ? buf->line_of(if_kw_pos) : 0;

    auto parse_cond_and_body = [&](ccx_node::if_branch& br) {
        skip_ws(s);
        if (s.peek() != '(')
            fail(s, "expected '(' after 'if'");
        s.advance();
        std::ostringstream cond;
        cap(cond, capture_kind::paren);
        br.cond_cpp = cond.str();
        std::vector<ccx_node> body;
        parse_block_body(s, comps, cap, body, buf);
        br.body = std::move(body);
    };

    {
        ccx_node::if_branch first;
        first.source_line = n.source_line;
        parse_cond_and_body(first);
        n.branches.push_back(std::move(first));
    }

    while (true) {
        size_t save = s.pos();
        skip_ws(s);
        if (!scanner::is_ident_start(s.peek())) {
            s.set_pos(save);
            break;
        }
        size_t else_pos = s.pos();
        auto id = s.read_identifier();
        if (id != "else") {
            s.set_pos(save);
            break;
        }
        size_t else_line = buf ? buf->line_of(else_pos) : 0;
        skip_ws(s);
        if (scanner::is_ident_start(s.peek())) {
            auto id2 = s.read_identifier();
            if (id2 != "if")
                fail(s, "expected 'if' or '{' after 'else'");
            ccx_node::if_branch br;
            br.source_line = else_line;
            parse_cond_and_body(br);
            n.branches.push_back(std::move(br));
            continue;
        }
        if (s.peek() != '{')
            fail(s, "expected '{' or 'if' after 'else'");
        ccx_node::if_branch else_br;
        else_br.is_else = true;
        else_br.source_line = else_line;
        std::vector<ccx_node> body;
        parse_block_body(s, comps, cap, body, buf);
        else_br.body = std::move(body);
        n.branches.push_back(std::move(else_br));
        break;
    }
    return n;
}

ccx_node parse_for_child(scanner& s,
                         const std::unordered_set<std::string>& comps,
                         const brace_capture_fn& cap,
                         const source_buffer* buf,
                         size_t kw_pos) {
    ccx_node n;
    n.k = ccx_node::kind::for_loop;
    n.source_line = buf ? buf->line_of(kw_pos) : 0;
    skip_ws(s);
    if (s.peek() != '(')
        fail(s, "expected '(' after 'for'");
    s.advance();
    std::ostringstream head;
    cap(head, capture_kind::paren);
    n.head_cpp = head.str();
    std::vector<ccx_node> body;
    parse_block_body(s, comps, cap, body, buf);
    n.children = std::move(body);
    return n;
}

ccx_node parse_while_child(scanner& s,
                           const std::unordered_set<std::string>& comps,
                           const brace_capture_fn& cap,
                           const source_buffer* buf,
                           size_t kw_pos) {
    ccx_node n;
    n.k = ccx_node::kind::while_loop;
    n.source_line = buf ? buf->line_of(kw_pos) : 0;
    skip_ws(s);
    if (s.peek() != '(')
        fail(s, "expected '(' after 'while'");
    s.advance();
    std::ostringstream head;
    cap(head, capture_kind::paren);
    n.head_cpp = head.str();
    std::vector<ccx_node> body;
    parse_block_body(s, comps, cap, body, buf);
    n.children = std::move(body);
    return n;
}

ccx_node parse_brace_child(scanner& s,
                           const std::unordered_set<std::string>& comps,
                           const brace_capture_fn& cap,
                           const source_buffer* buf) {
    // Precondition: s.peek() == '{'
    size_t open_pos = s.pos();
    s.advance();
    size_t after_open = s.pos();
    skip_ws(s);
    if (scanner::is_ident_start(s.peek())) {
        size_t kw_pos = s.pos();
        auto id = s.read_identifier();
        ccx_node result;
        bool handled = false;
        if (id == "if") {
            result = parse_if_child(s, comps, cap, buf, kw_pos);
            handled = true;
        } else if (id == "for") {
            result = parse_for_child(s, comps, cap, buf, kw_pos);
            handled = true;
        } else if (id == "while") {
            result = parse_while_child(s, comps, cap, buf, kw_pos);
            handled = true;
        }
        if (handled) {
            skip_ws(s);
            if (s.peek() != '}')
                fail(s, "expected '}' after control-flow block");
            s.advance();
            return result;
        }
        s.set_pos(after_open);
    }
    ccx_node n;
    n.k = ccx_node::kind::expr_child;
    n.source_line = buf ? buf->line_of(open_pos) : 0;
    std::ostringstream oss;
    cap(oss, capture_kind::brace);
    n.expr_text = oss.str();
    return n;
}

ccx_attr parse_attr(scanner& s, const brace_capture_fn& cap) {
    ccx_attr a;
    a.name = std::string(s.read_identifier());
    if (a.name.empty())
        fail(s, "expected attribute name");
    skip_ws(s);
    if (s.peek() != '=') {
        a.has_value = false;
        return a;
    }
    s.advance();  // =
    skip_ws(s);
    if (s.peek() == '"') {
        a.value_cpp = std::string(s.read_string_literal());
        a.is_literal = true;
    } else if (s.peek() == '{') {
        s.advance();  // past {
        std::ostringstream oss;
        cap(oss, capture_kind::brace);
        a.value_cpp = oss.str();
        a.is_literal = false;
    } else {
        fail(s, "expected '\"' or '{' after '=' in attribute");
    }
    return a;
}

bool is_all_whitespace(const std::string& raw) {
    for (char c : raw)
        if (!scanner::is_whitespace(c))
            return false;
    return true;
}

struct trim_info {
    size_t pos;
    bool has_newline;
};

trim_info trim_leading_ws(const std::string& raw) {
    size_t i = 0;
    bool has_nl = false;
    while (i < raw.size() && scanner::is_whitespace(raw[i])) {
        if (raw[i] == '\n' || raw[i] == '\r')
            has_nl = true;
        ++i;
    }
    return {i, has_nl};
}

trim_info trim_trailing_ws(const std::string& raw, size_t from) {
    size_t j = raw.size();
    bool has_nl = false;
    while (j > from && scanner::is_whitespace(raw[j - 1])) {
        if (raw[j - 1] == '\n' || raw[j - 1] == '\r')
            has_nl = true;
        --j;
    }
    return {j, has_nl};
}

// Join non-empty lines with a single space, stripping per-line leading whitespace.
std::string collapse_line_runs(const std::string& inner) {
    std::string out;
    size_t p = 0;
    bool first = true;
    while (p <= inner.size()) {
        size_t nl = inner.find('\n', p);
        std::string line = (nl == std::string::npos) ? inner.substr(p) : inner.substr(p, nl - p);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        size_t lt = 0;
        while (lt < line.size() && (line[lt] == ' ' || line[lt] == '\t'))
            lt++;
        std::string trimmed = line.substr(lt);
        if (!trimmed.empty()) {
            if (!first)
                out += ' ';
            out += trimmed;
            first = false;
        }
        if (nl == std::string::npos)
            break;
        p = nl + 1;
    }
    return out;
}

std::string normalize_text(const std::string& raw) {
    if (is_all_whitespace(raw))
        return {};
    auto [leading_start, leading_nl] = trim_leading_ws(raw);
    auto [trailing_end, trailing_nl] = trim_trailing_ws(raw, leading_nl ? leading_start : 0);
    size_t start = leading_nl ? leading_start : 0;
    size_t end = trailing_nl ? trailing_end : raw.size();
    std::string inner = raw.substr(start, end - start);
    if (!leading_nl && !trailing_nl)
        return inner;
    return collapse_line_runs(inner);
}

std::string read_text_run(scanner& s, bool stop_at_rbrace) {
    std::string out;
    while (!s.eof()) {
        char c = s.peek();
        if (c == '<' || c == '{')
            break;
        if (c == '}' && stop_at_rbrace)
            break;
        out += c;
        s.advance();
    }
    return out;
}

std::vector<ccx_node> parse_children(scanner& s,
                                     const std::unordered_set<std::string>& comps,
                                     const brace_capture_fn& cap,
                                     bool terminator_is_rbrace,
                                     std::string_view element_name,
                                     const source_buffer* buf) {
    std::vector<ccx_node> children;
    while (!s.eof()) {
        char c = s.peek();
        if (c == '<') {
            if (s.peek(1) == '/') {
                if (terminator_is_rbrace)
                    fail(s, "unexpected closing tag in block body");
                s.advance(2);
                skip_ws(s);
                std::string name = read_tag_name(s);
                skip_ws(s);
                if (s.peek() != '>')
                    fail(s, "expected '>' in closing tag");
                s.advance();
                if (name != element_name)
                    fail(s, "mismatched closing tag");
                return children;
            }
            children.push_back(parse_element(s, comps, cap, buf));
        } else if (c == '{') {
            children.push_back(parse_brace_child(s, comps, cap, buf));
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
    if (terminator_is_rbrace)
        return children;
    fail(s, "unexpected end of input inside element children");
}

}  // namespace

ccx_node parse_element(scanner& s, const std::unordered_set<std::string>& comps, const brace_capture_fn& cap, const source_buffer* buf) {
    if (s.peek() != '<')
        fail(s, "expected '<' at element start");
    size_t element_pos = s.pos();
    s.advance();
    skip_ws(s);
    ccx_node n;
    n.k = ccx_node::kind::element;
    n.source_line = buf ? buf->line_of(element_pos) : 0;
    n.tag_name = read_tag_name(s);
    n.is_component = comps.count(n.tag_name) > 0;
    while (true) {
        skip_ws(s);
        char c = s.peek();
        if (c == '/' || c == '>')
            break;
        if (c == '\0')
            fail(s, "unexpected end of input inside tag");
        n.attrs.push_back(parse_attr(s, cap));
    }
    if (s.peek() == '/') {
        s.advance();
        if (s.peek() != '>')
            fail(s, "expected '>' after '/' in self-closing tag");
        s.advance();
        return n;
    }
    if (s.peek() != '>')
        fail(s, "expected '>' at end of opening tag");
    s.advance();
    n.children = parse_children(s, comps, cap, /*terminator_is_rbrace=*/false, n.tag_name, buf);
    return n;
}

}  // namespace weasel::compiler
