#include <doctest.h>
#include <string>
#include "weasel/compiler/boundary.hpp"

using weasel::compiler::find_ccx_spans;
using weasel::compiler::is_position_in_ccx;

TEST_CASE("find_ccx_spans: no CCX returns empty") {
    auto v = find_ccx_spans("int main() { return 0; }\n");
    CHECK(v.empty());
}

TEST_CASE("find_ccx_spans: single self-closing tag") {
    std::string src = "node f() { return <br/>; }\n";
    auto v = find_ccx_spans(src);
    REQUIRE(v.size() == 1);
    CHECK(src[v[0].begin] == '<');
    // Span should cover through '>' of /> — at least through the tag.
    CHECK(v[0].end > v[0].begin);
    CHECK(v[0].end <= src.size());
}

TEST_CASE("find_ccx_spans: element with children") {
    std::string src = "node f() { return <div><span>hi</span></div>; }\n";
    auto v = find_ccx_spans(src);
    REQUIRE(v.size() == 1);
    // The entire <div>...</div> is one CCX region.
    CHECK(src.substr(v[0].begin, v[0].end - v[0].begin).starts_with("<div>"));
}

TEST_CASE("is_position_in_ccx: inside returns true") {
    std::string src = "node f() { return <div>hi</div>; }\n";
    // Offset of 'd' in "div"
    size_t off = src.find("<div>") + 1;
    CHECK(is_position_in_ccx(src, off));
}

TEST_CASE("is_position_in_ccx: outside returns false") {
    std::string src = "node f() { return <div>hi</div>; }\n";
    // Offset of 'f' in function name
    size_t off = src.find("f()");
    CHECK_FALSE(is_position_in_ccx(src, off));
}

TEST_CASE("is_position_in_ccx: operator '<' not CCX") {
    std::string src = "int f(int a, int b) { return a < b; }\n";
    size_t off = src.find("a <");
    CHECK_FALSE(is_position_in_ccx(src, off));
    CHECK_FALSE(is_position_in_ccx(src, off + 2));  // position on '<'
}

TEST_CASE("is_position_in_ccx: shift operator not CCX") {
    std::string src = "int f() { return 1 << 4; }\n";
    CHECK_FALSE(is_position_in_ccx(src, src.find("<<")));
}

TEST_CASE("is_position_in_ccx: multi-region source") {
    std::string src =
        "node a() { return <p>A</p>; }\n"
        "int filler() { return 0; }\n"
        "node b() { return <p>B</p>; }\n";
    auto spans = find_ccx_spans(src);
    REQUIRE(spans.size() == 2);
    CHECK(is_position_in_ccx(src, src.find("<p>A")));
    CHECK(is_position_in_ccx(src, src.find("<p>B")));
    CHECK_FALSE(is_position_in_ccx(src, src.find("filler")));
}

TEST_CASE("is_position_in_ccx: end offset is exclusive") {
    std::string src = "node f() { return <br/>; }\n";
    auto spans = find_ccx_spans(src);
    REQUIRE(spans.size() == 1);
    CHECK_FALSE(is_position_in_ccx(src, spans[0].end));
    CHECK(is_position_in_ccx(src, spans[0].end - 1));
}
