#include <doctest.h>
#include "weasel/compiler/source.hpp"

using weasel::compiler::make_source;

TEST_CASE("position_of: empty buffer") {
    auto buf = make_source("x", "");
    auto p = buf.position_of(0);
    CHECK(p.line == 1);
    CHECK(p.column == 0);
}

TEST_CASE("position_of: single line") {
    auto buf = make_source("x", "hello");
    CHECK(buf.position_of(0).line == 1);
    CHECK(buf.position_of(0).column == 0);
    CHECK(buf.position_of(4).line == 1);
    CHECK(buf.position_of(4).column == 4);
}

TEST_CASE("position_of: newline boundary") {
    auto buf = make_source("x", "ab\ncd");
    // 'a' at offset 0
    CHECK(buf.position_of(0).line == 1);
    CHECK(buf.position_of(0).column == 0);
    // 'b' at offset 1
    CHECK(buf.position_of(1).line == 1);
    CHECK(buf.position_of(1).column == 1);
    // '\n' at offset 2 — still counted on line 1
    CHECK(buf.position_of(2).line == 1);
    CHECK(buf.position_of(2).column == 2);
    // 'c' at offset 3 — first char of line 2
    CHECK(buf.position_of(3).line == 2);
    CHECK(buf.position_of(3).column == 0);
    // 'd' at offset 4
    CHECK(buf.position_of(4).line == 2);
    CHECK(buf.position_of(4).column == 1);
}

TEST_CASE("position_of: past EOF returns last line") {
    auto buf = make_source("x", "ab\ncd");
    auto p = buf.position_of(100);
    CHECK(p.line == 2);
    CHECK(p.column == 97);
}

TEST_CASE("position_of: multiple blank lines") {
    auto buf = make_source("x", "a\n\n\nb");
    CHECK(buf.position_of(0).line == 1);
    CHECK(buf.position_of(2).line == 2);  // first blank
    CHECK(buf.position_of(2).column == 0);
    CHECK(buf.position_of(3).line == 3);
    CHECK(buf.position_of(4).line == 4);
    CHECK(buf.position_of(4).column == 0);
}

TEST_CASE("column_of agrees with position_of") {
    auto buf = make_source("x", "one\ntwo\nthree\n");
    for (size_t i = 0; i < buf.text.size(); ++i) {
        CHECK(buf.column_of(i) == buf.position_of(i).column);
        CHECK(buf.line_of(i) == buf.position_of(i).line);
    }
}
