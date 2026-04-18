#include "weasel/compiler/transpiler.hpp"
#include "weasel/compiler/emitter.hpp"
#include "weasel/compiler/ccx_parser.hpp"
#include "weasel/compiler/scanner.hpp"
#include <ostream>

namespace weasel::compiler {
namespace {

enum class prev_tok {
    none,
    ident,
    number,
    string_lit,
    rparen,
    rbracket,
    rbrace,
    rangle,
    kw_return_like,
    punct_expr_start,
};

class driver {
  public:
    driver(std::string_view src, std::ostream &out, const std::unordered_set<std::string> &components)
        : s_(src), out_(&out), components_(components) {}

    void run() {
        while (!s_.eof())
            step();
    }

    // Capture a `{...}` expression: precondition: s_.peek() == '{'? No —
    // the caller has already advanced past the opening '{'. We read until
    // the matching '}' at depth 0, writing transformed C++ to `out_capture`.
    void capture_brace(std::ostream &out_capture) {
        int depth = 1;
        std::ostream *saved = out_;
        prev_tok saved_prev = prev_;
        out_ = &out_capture;
        prev_ = prev_tok::punct_expr_start; // we're just past '{'
        while (!s_.eof() && depth > 0) {
            char c = s_.peek();
            if (c == '{') {
                *out_ << c;
                s_.advance();
                depth++;
                prev_ = prev_tok::punct_expr_start;
                continue;
            }
            if (c == '}') {
                depth--;
                if (depth == 0) {
                    s_.advance();
                    break;
                }
                *out_ << c;
                s_.advance();
                prev_ = prev_tok::rbrace;
                continue;
            }
            step();
        }
        out_ = saved;
        prev_ = saved_prev;
    }

    // Capture balanced parens. Precondition: caller has advanced past '('.
    // Writes inner content transformed to `out_capture`, advances past ')'.
    void capture_paren(std::ostream &out_capture) {
        int depth = 1;
        std::ostream *saved = out_;
        prev_tok saved_prev = prev_;
        out_ = &out_capture;
        prev_ = prev_tok::punct_expr_start;
        while (!s_.eof() && depth > 0) {
            char c = s_.peek();
            if (c == '(') {
                *out_ << c;
                s_.advance();
                depth++;
                prev_ = prev_tok::punct_expr_start;
                continue;
            }
            if (c == ')') {
                depth--;
                if (depth == 0) {
                    s_.advance();
                    break;
                }
                *out_ << c;
                s_.advance();
                prev_ = prev_tok::rparen;
                continue;
            }
            step();
        }
        out_ = saved;
        prev_ = saved_prev;
    }

  private:
    void step() {
        if (should_enter_ccx()) {
            parse_and_emit_ccx();
            return;
        }

        char c = s_.peek();

        // Preprocessor line (only at line start)
        if (c == '#' && s_.at_line_start()) {
            auto line = s_.read_preprocessor_line();
            *out_ << line;
            return;
        }

        // Line / block comment
        if (c == '/' && s_.peek(1) == '/') {
            *out_ << s_.read_line_comment();
            return;
        }
        if (c == '/' && s_.peek(1) == '*') {
            *out_ << s_.read_block_comment();
            return;
        }

        // Raw string
        if (c == 'R' && s_.peek(1) == '"') {
            *out_ << s_.read_raw_string_literal();
            prev_ = prev_tok::string_lit;
            return;
        }

        // String / char literal
        if (c == '"') {
            *out_ << s_.read_string_literal();
            prev_ = prev_tok::string_lit;
            return;
        }
        if (c == '\'') {
            *out_ << s_.read_char_literal();
            prev_ = prev_tok::string_lit;
            return;
        }

        // Identifier
        if (scanner::is_ident_start(c)) {
            auto id = s_.read_identifier();
            handle_identifier(id);
            return;
        }

        // Number
        if (scanner::is_digit(c)) {
            *out_ << s_.read_number();
            prev_ = prev_tok::number;
            return;
        }

        // Whitespace
        if (scanner::is_whitespace(c)) {
            *out_ << c;
            s_.advance();
            return;
        }

        // Punctuation (single char)
        *out_ << c;
        s_.advance();
        update_prev_from_char(c);
    }

    void handle_identifier(std::string_view id) {
        if (id == "component") {
            *out_ << "weasel::node";
            prev_ = prev_tok::ident;
            return;
        }
        *out_ << id;
        if (id == "return" || id == "co_return" || id == "throw" || id == "co_yield") {
            prev_ = prev_tok::kw_return_like;
        } else if (id == "if" || id == "else" || id == "for" || id == "while" || id == "do" || id == "switch" || id == "case" ||
                   id == "default" || id == "new" || id == "delete") {
            prev_ = prev_tok::punct_expr_start;
        } else {
            prev_ = prev_tok::ident;
        }
    }

    void update_prev_from_char(char c) {
        switch (c) {
        case ';':
        case '(':
        case ',':
        case '{':
        case '=':
        case '?':
        case ':':
        case '+':
        case '-':
        case '*':
        case '/':
        case '%':
        case '!':
        case '&':
        case '|':
        case '^':
        case '~':
        case '<':
            prev_ = prev_tok::punct_expr_start;
            break;
        case ')':
            prev_ = prev_tok::rparen;
            break;
        case ']':
            prev_ = prev_tok::rbracket;
            break;
        case '}':
            prev_ = prev_tok::rbrace;
            break;
        case '>':
            prev_ = prev_tok::rangle;
            break;
        default:
            break;
        }
    }

    bool should_enter_ccx() {
        if (s_.peek() != '<')
            return false;
        char next = s_.peek(1);
        if (next == '=' || next == '<')
            return false;
        if (!scanner::is_ident_start(next))
            return false;
        switch (prev_) {
        case prev_tok::kw_return_like:
        case prev_tok::punct_expr_start:
        case prev_tok::rbrace:
        case prev_tok::none:
            return true;
        default:
            return false;
        }
    }

    void parse_and_emit_ccx() {
        size_t ccx_start = s_.pos();
        brace_capture_fn cap_brace = [this](std::ostream &o) { this->capture_brace(o); };
        // Attribute values and control-flow heads are captured via parenthesis/brace.
        // ccx_parser uses the `cap` callback with the convention:
        // after seeing `(` (for if/for/while head) or `{` (for attr/expr child),
        // it advances past the opener and calls cap. We need two behaviors.
        // Current parser usage:
        //   parse_attr: advances past '{' then calls cap → brace capture
        //   parse_if/for/while: advances past '(' then calls cap → paren capture
        //   parse_brace_child: advances past '{' then calls cap → brace capture
        // So `cap` must handle both. We distinguish by what char preceded —
        // track that via a simple: at invocation time, inspect the scanner's
        // prior char. If it's `(`, do paren capture; if `{`, do brace capture.
        auto smart_cap = [this](std::ostream &o) {
            // Look back one char to decide mode.
            size_t p = this->s_.pos();
            char prev_char = (p > 0) ? this->s_.text()[p - 1] : '\0';
            if (prev_char == '(')
                this->capture_paren(o);
            else
                this->capture_brace(o);
        };
        ccx_node root = parse_element(s_, components_, smart_cap);
        emit(root, *out_);
        // After emission, we're after a closed C++ expression.
        prev_ = prev_tok::rparen;
        // Line-preservation: pad output with newlines if the JSX source span
        // contained more newlines than the emitted replacement.
        size_t consumed_lines = 0;
        for (size_t i = ccx_start; i < s_.pos(); ++i) {
            if (s_.text()[i] == '\n')
                consumed_lines++;
        }
        // We haven't counted newlines in the emitted text, so as a simple
        // strategy pad with `consumed_lines` newlines. This may over-pad if
        // the emitter inserted newlines itself (it does not currently).
        for (size_t i = 0; i < consumed_lines; ++i)
            *out_ << '\n';
    }

    scanner s_;
    std::ostream *out_;
    const std::unordered_set<std::string> &components_;
    prev_tok prev_ = prev_tok::none;
};

} // namespace

std::unordered_set<std::string> collect_components(std::string_view src) {
    std::unordered_set<std::string> out;
    scanner s(src);
    while (!s.eof()) {
        char c = s.peek();
        if (c == '#' && s.at_line_start()) {
            s.read_preprocessor_line();
            continue;
        }
        if (c == '/' && s.peek(1) == '/') {
            s.read_line_comment();
            continue;
        }
        if (c == '/' && s.peek(1) == '*') {
            s.read_block_comment();
            continue;
        }
        if (c == 'R' && s.peek(1) == '"') {
            s.read_raw_string_literal();
            continue;
        }
        if (c == '"') {
            s.read_string_literal();
            continue;
        }
        if (c == '\'') {
            s.read_char_literal();
            continue;
        }
        if (scanner::is_ident_start(c)) {
            auto id = s.read_identifier();
            if (id == "component") {
                s.read_whitespace();
                auto name = s.read_identifier();
                if (!name.empty()) {
                    size_t save = s.pos();
                    s.read_whitespace();
                    if (s.peek() == '(') {
                        out.insert(std::string(name));
                    } else {
                        s.set_pos(save);
                    }
                }
            }
            continue;
        }
        if (scanner::is_digit(c)) {
            s.read_number();
            continue;
        }
        s.advance();
    }
    return out;
}

void transpile(std::string_view src, std::ostream &out, const transpile_options &) {
    auto components = collect_components(src);
    driver d(src, out, components);
    d.run();
}

} // namespace weasel::compiler
