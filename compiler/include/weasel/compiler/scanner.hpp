#pragma once
#include <cstddef>
#include <string_view>

namespace weasel::compiler {

class scanner {
public:
    explicit scanner(std::string_view text) : text_(text) {}

    size_t pos() const { return pos_; }
    bool eof() const { return pos_ >= text_.size(); }
    char peek(size_t off = 0) const {
        return (pos_ + off < text_.size()) ? text_[pos_ + off] : '\0';
    }
    bool starts_with(std::string_view s) const {
        if (pos_ + s.size() > text_.size()) return false;
        return text_.compare(pos_, s.size(), s) == 0;
    }
    void advance(size_t n = 1) {
        pos_ = (pos_ + n > text_.size()) ? text_.size() : pos_ + n;
    }
    void set_pos(size_t p) {
        pos_ = (p > text_.size()) ? text_.size() : p;
    }
    std::string_view view(size_t start, size_t end) const {
        return text_.substr(start, end - start);
    }
    std::string_view text() const { return text_; }

    std::string_view read_identifier();
    std::string_view read_number();
    std::string_view read_string_literal();
    std::string_view read_char_literal();
    std::string_view read_raw_string_literal();
    std::string_view read_line_comment();
    std::string_view read_block_comment();
    std::string_view read_preprocessor_line();
    std::string_view read_whitespace();

    bool at_line_start() const;

    // Character classifiers are ASCII-only. Non-ASCII bytes (e.g. UTF-8 code
    // points in identifiers) are not recognised as identifier characters and
    // will cause the scanner to emit spurious tokens. Weasel source files must
    // use ASCII-only identifiers; UTF-8 is only valid inside string literals
    // and comments.
    static bool is_ident_start(char c);
    static bool is_ident_cont(char c);
    static bool is_digit(char c);
    static bool is_hspace(char c);
    static bool is_whitespace(char c);

private:
    std::string_view text_;
    size_t pos_ = 0;
};

} // namespace weasel::compiler
