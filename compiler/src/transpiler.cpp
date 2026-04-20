#include "weasel/compiler/transpiler.hpp"
#include "weasel/compiler/emitter.hpp"
#include "weasel/compiler/ccx_parser.hpp"
#include "weasel/compiler/scanner.hpp"
#include "weasel/compiler/source.hpp"
#include <algorithm>
#include <ostream>
#include <streambuf>

namespace weasel::compiler {
namespace {

// Wraps a destination streambuf and counts the current 1-based line number
// based on '\n' characters written through it.
class counting_streambuf : public std::streambuf {
  public:
    explicit counting_streambuf(std::streambuf* dest) : dest_(dest) {}
    size_t line() const { return line_; }

  protected:
    int_type overflow(int_type ch) override {
        if (ch == traits_type::eof()) return traits_type::not_eof(ch);
        char c = static_cast<char>(ch);
        if (c == '\n') ++line_;
        return dest_->sputc(c) == traits_type::eof() ? traits_type::eof() : ch;
    }
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        for (std::streamsize i = 0; i < n; ++i) {
            if (s[i] == '\n') ++line_;
        }
        return dest_->sputn(s, n);
    }

  private:
    std::streambuf* dest_;
    size_t line_ = 1;
};

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
    driver(std::string_view src, std::ostream &out, counting_streambuf *counter,
           const std::unordered_set<std::string> &components,
           const transpile_options &opts,
           std::vector<line_span> *line_map, const source_buffer *buf)
        : s_(src), out_(&out), counter_(counter), components_(components),
          opts_(&opts), line_map_(line_map), buf_(buf) {}

    void run() {
        while (!s_.eof())
            step();
        // Emit a trailing cpp_passthrough span covering anything after the
        // last CCX region (or the whole file if there were no CCX regions).
        if (line_map_ && counter_ && buf_) {
            size_t end_cc = counter_->line();
            size_t end_weasel = buf_->line_of(s_.pos() > 0 ? s_.pos() - 1 : 0);
            if (end_cc >= cc_line_cursor_) {
                line_span span{
                    weasel_line_cursor_,
                    std::max(weasel_line_cursor_, end_weasel),
                    cc_line_cursor_,
                    end_cc,
                    span_kind::cpp_passthrough,
                };
                // Avoid pushing a zero-width span past EOF on empty input.
                if (span.cc_line_end >= span.cc_line_begin)
                    line_map_->push_back(span);
            }
        }
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
        size_t ccx_weasel_line_begin = buf_ ? buf_->line_of(ccx_start) : 0;
        size_t ccx_cc_line_begin = counter_ ? counter_->line() : 0;
        if (line_map_ && counter_ && buf_) {
            // Flush any pending cpp_passthrough span covering cc lines since
            // the last boundary up to (but not including) the CCX start.
            if (ccx_cc_line_begin > cc_line_cursor_) {
                size_t weasel_end = ccx_weasel_line_begin > weasel_line_cursor_
                                        ? ccx_weasel_line_begin - 1
                                        : weasel_line_cursor_;
                line_map_->push_back(line_span{
                    weasel_line_cursor_,
                    weasel_end,
                    cc_line_cursor_,
                    ccx_cc_line_begin - 1,
                    span_kind::cpp_passthrough,
                });
            }
        }
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
        ccx_node root = parse_element(s_, components_, smart_cap, buf_);
        size_t cc_line_before_emit = counter_ ? counter_->line() : 0;
        emit(root, *out_, buf_ ? ccx_cc_line_begin : 0);
        size_t cc_line_after_emit = counter_ ? counter_->line() : 0;
        size_t emitted_newlines = cc_line_after_emit - cc_line_before_emit;
        if (opts_ && opts_->on_ccx_span)
            opts_->on_ccx_span(ccx_start, s_.pos());
        // After emission, we're after a closed C++ expression.
        prev_ = prev_tok::rparen;
        // Line-preservation: pad output with newlines so that the total
        // newlines emitted for this CCX region equals the number of newlines
        // consumed in the source. The emitter may have already emitted some.
        size_t consumed_lines = 0;
        for (size_t i = ccx_start; i < s_.pos(); ++i) {
            if (s_.text()[i] == '\n')
                consumed_lines++;
        }
        size_t pad = consumed_lines > emitted_newlines ? consumed_lines - emitted_newlines : 0;
        for (size_t i = 0; i < pad; ++i)
            *out_ << '\n';

        if (line_map_ && counter_ && buf_) {
            size_t ccx_cc_line_end = counter_->line();
            size_t last_char_pos = s_.pos() > ccx_start ? s_.pos() - 1 : ccx_start;
            size_t ccx_weasel_line_end = buf_->line_of(last_char_pos);
            line_map_->push_back(line_span{
                ccx_weasel_line_begin,
                ccx_weasel_line_end,
                ccx_cc_line_begin,
                ccx_cc_line_end,
                span_kind::ccx_region,
            });
            cc_line_cursor_ = ccx_cc_line_end + 1;
            weasel_line_cursor_ = ccx_weasel_line_end + 1;
        }
    }

    scanner s_;
    std::ostream *out_;
    counting_streambuf *counter_;
    const std::unordered_set<std::string> &components_;
    const transpile_options *opts_;
    std::vector<line_span> *line_map_;
    const source_buffer *buf_;
    size_t cc_line_cursor_ = 1;
    size_t weasel_line_cursor_ = 1;
    prev_tok prev_ = prev_tok::none;
};

} // namespace

namespace {
// Skip a balanced (...) starting at scanner position on '(' (consumed).
// Returns the raw text between the parens (exclusive). Scanner advances past ')'.
// If EOF is hit first, returns whatever was read; scanner is left at EOF.
std::string capture_balanced_parens(scanner &s) {
    // Precondition: s.peek() == '('
    s.advance(); // consume '('
    size_t begin = s.pos();
    int depth = 1;
    while (!s.eof() && depth > 0) {
        char c = s.peek();
        if (c == '/' && s.peek(1) == '/') {
            s.read_line_comment();
            continue;
        }
        if (c == '/' && s.peek(1) == '*') {
            s.read_block_comment();
            continue;
        }
        if (c == '"') { s.read_string_literal(); continue; }
        if (c == '\'') { s.read_char_literal(); continue; }
        if (c == 'R' && s.peek(1) == '"') { s.read_raw_string_literal(); continue; }
        if (c == '(') { depth++; s.advance(); continue; }
        if (c == ')') {
            depth--;
            if (depth == 0) {
                size_t end = s.pos();
                s.advance();
                return std::string(s.text().substr(begin, end - begin));
            }
            s.advance();
            continue;
        }
        s.advance();
    }
    return std::string(s.text().substr(begin, s.pos() - begin));
}
} // namespace

std::vector<component_info> collect_component_infos(std::string_view src) {
    std::vector<component_info> out;
    source_buffer buf = make_source("", std::string(src));
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
                size_t name_offset = s.pos();
                auto name = s.read_identifier();
                if (!name.empty()) {
                    size_t save = s.pos();
                    s.read_whitespace();
                    if (s.peek() == '(') {
                        std::string params = capture_balanced_parens(s);
                        auto pos = buf.position_of(name_offset);
                        out.push_back(component_info{
                            std::string(name),
                            name_offset,
                            pos.line,
                            pos.column,
                            std::move(params),
                        });
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

std::unordered_set<std::string> collect_components(std::string_view src) {
    std::unordered_set<std::string> out;
    for (const auto &c : collect_component_infos(src)) out.insert(c.name);
    return out;
}

void transpile(std::string_view src, std::ostream &out, const transpile_options &opts) {
    auto components = collect_components(src);
    driver d(src, out, nullptr, components, opts, nullptr, nullptr);
    d.run();
}

transpile_result transpile_with_map(std::string_view src, std::ostream &out,
                                    const transpile_options &opts) {
    transpile_result r;
    r.components = collect_component_infos(src);
    source_buffer buf = make_source("", std::string(src));
    auto components = collect_components(src);

    counting_streambuf counter(out.rdbuf());
    std::ostream counted(&counter);
    try {
        driver d(src, counted, &counter, components, opts, &r.line_map, &buf);
        d.run();
        counted.flush();
        r.ok = true;
    } catch (const parse_error &e) {
        counted.flush();
        r.ok = false;
        r.diagnostics.push_back(e.diag);
    }
    return r;
}

cc_to_weasel_result cc_line_to_weasel(const std::vector<line_span> &map, size_t cc_line) {
    // Binary search for the span whose [cc_line_begin, cc_line_end] covers cc_line.
    auto it = std::upper_bound(map.begin(), map.end(), cc_line,
                               [](size_t line, const line_span &sp) { return line < sp.cc_line_begin; });
    if (it == map.begin()) return {0, nullptr};
    --it;
    if (cc_line > it->cc_line_end) return {0, nullptr};
    size_t weasel_line = it->kind == span_kind::ccx_region
                             ? it->weasel_line_begin
                             : it->weasel_line_begin + (cc_line - it->cc_line_begin);
    if (weasel_line > it->weasel_line_end) weasel_line = it->weasel_line_end;
    return {weasel_line, &*it};
}

} // namespace weasel::compiler
