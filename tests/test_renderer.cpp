#include "doctest.h"
#include "weasel/factory.hpp"
#include "weasel/renderer.hpp"
#include <sstream>

using namespace weasel;

TEST_CASE("text_node is HTML-escaped") {
    CHECK(render_to_string(text("<b>")) == "&lt;b&gt;");
    CHECK(render_to_string(text("a & b")) == "a &amp; b");
}

TEST_CASE("raw_node passes through unescaped") {
    CHECK(render_to_string(raw("<b>bold</b>")) == "<b>bold</b>");
}

TEST_CASE("monostate renders nothing") {
    node n;
    CHECK(render_to_string(n).empty());
}

TEST_CASE("element with text child") {
    auto n = tag("p", {}, {text("hello")});
    CHECK(render_to_string(n) == "<p>hello</p>");
}

TEST_CASE("element with attributes") {
    auto n = tag("div", {{"class", "box"}, {"id", "main"}});
    CHECK(render_to_string(n) == R"(<div class="box" id="main"></div>)");
}

TEST_CASE("attribute values are escaped") {
    auto n = tag("input", {{"value", R"(say "hi")"}});
    CHECK(render_to_string(n) == R"(<input value="say &quot;hi&quot;">)");
}

TEST_CASE("void elements have no closing tag") {
    CHECK(render_to_string(tag("br"))  == "<br>");
    CHECK(render_to_string(tag("hr"))  == "<hr>");
    CHECK(render_to_string(tag("img", {{"src", "x.png"}})) == R"(<img src="x.png">)");
    CHECK(render_to_string(tag("input", {{"type", "text"}})) == R"(<input type="text">)");
}

TEST_CASE("nested tree") {
    auto n = tag("ul", {}, {
        tag("li", {}, {text("one")}),
        tag("li", {}, {text("two")})
    });
    CHECK(render_to_string(n) == "<ul><li>one</li><li>two</li></ul>");
}

TEST_CASE("fragment renders children without wrapper") {
    auto n = fragment({text("a"), text("b"), text("c")});
    CHECK(render_to_string(n) == "abc");
}

TEST_CASE("render to ostream") {
    std::ostringstream oss;
    render(tag("span", {}, {text("hi")}), oss);
    CHECK(oss.str() == "<span>hi</span>");
}
