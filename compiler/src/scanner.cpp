#include "weasel/compiler/scanner.hpp"

namespace weasel::compiler {

bool scanner::is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool scanner::is_ident_cont(char c) {
    return is_ident_start(c) || is_digit(c);
}
bool scanner::is_digit(char c) { return c >= '0' && c <= '9'; }
bool scanner::is_hspace(char c) { return c == ' ' || c == '\t'; }
bool scanner::is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool scanner::at_line_start() const {
    if (pos_ == 0) return true;
    return text_[pos_ - 1] == '\n';
}

std::string_view scanner::read_identifier() {
    if (!is_ident_start(peek())) return {};
    size_t start = pos_;
    while (is_ident_cont(peek())) advance();
    return view(start, pos_);
}

std::string_view scanner::read_number() {
    if (!is_digit(peek())) return {};
    size_t start = pos_;
    while (is_ident_cont(peek()) || peek() == '.' || peek() == '\'') advance();
    return view(start, pos_);
}

std::string_view scanner::read_string_literal() {
    if (peek() != '"') return {};
    size_t start = pos_;
    advance();
    while (!eof() && peek() != '"' && peek() != '\n') {
        if (peek() == '\\' && pos_ + 1 < text_.size()) advance(2);
        else advance();
    }
    if (peek() == '"') advance();
    return view(start, pos_);
}

std::string_view scanner::read_char_literal() {
    if (peek() != '\'') return {};
    size_t start = pos_;
    advance();
    while (!eof() && peek() != '\'' && peek() != '\n') {
        if (peek() == '\\' && pos_ + 1 < text_.size()) advance(2);
        else advance();
    }
    if (peek() == '\'') advance();
    return view(start, pos_);
}

std::string_view scanner::read_raw_string_literal() {
    if (peek() != 'R' || peek(1) != '"') return {};
    size_t start = pos_;
    advance(2);
    size_t delim_start = pos_;
    while (!eof() && peek() != '(' && peek() != '\n') advance();
    if (peek() != '(') return view(start, pos_);
    std::string_view delim = view(delim_start, pos_);
    advance();
    while (!eof()) {
        if (peek() == ')') {
            size_t save = pos_;
            advance();
            bool match = true;
            for (char d : delim) {
                if (peek() != d) { match = false; break; }
                advance();
            }
            if (match && peek() == '"') { advance(); return view(start, pos_); }
            pos_ = save + 1;
            continue;
        }
        advance();
    }
    return view(start, pos_);
}

std::string_view scanner::read_line_comment() {
    if (peek() != '/' || peek(1) != '/') return {};
    size_t start = pos_;
    while (!eof() && peek() != '\n') advance();
    return view(start, pos_);
}

std::string_view scanner::read_block_comment() {
    if (peek() != '/' || peek(1) != '*') return {};
    size_t start = pos_;
    advance(2);
    while (!eof()) {
        if (peek() == '*' && peek(1) == '/') { advance(2); break; }
        advance();
    }
    return view(start, pos_);
}

std::string_view scanner::read_preprocessor_line() {
    if (peek() != '#') return {};
    size_t start = pos_;
    while (!eof()) {
        if (peek() == '\\' && (peek(1) == '\n' || (peek(1) == '\r' && peek(2) == '\n'))) {
            advance();
            if (peek() == '\r') advance();
            if (peek() == '\n') advance();
            continue;
        }
        if (peek() == '\n') break;
        advance();
    }
    return view(start, pos_);
}

std::string_view scanner::read_whitespace() {
    size_t start = pos_;
    while (is_whitespace(peek())) advance();
    return view(start, pos_);
}

} // namespace weasel::compiler
