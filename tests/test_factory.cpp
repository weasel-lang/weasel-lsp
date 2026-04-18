#include "doctest.h"
#include "weasel/factory.hpp"

using namespace weasel;

TEST_CASE("text() creates text_node") {
    auto n = text("hello");
    REQUIRE(std::holds_alternative<text_node>(n));
    CHECK(std::get<text_node>(n).content == "hello");
}

TEST_CASE("raw() creates raw_node") {
    auto n = raw("<em>emphasis</em>");
    REQUIRE(std::holds_alternative<raw_node>(n));
    CHECK(std::get<raw_node>(n).html == "<em>emphasis</em>");
}

TEST_CASE("fragment() creates fragment_node") {
    auto n = fragment({text("a"), text("b")});
    REQUIRE(std::holds_alternative<fragment_node>(n));
    CHECK(std::get<fragment_node>(n).children.size() == 2);
}

TEST_CASE("tag() with no attrs or children") {
    auto n = tag("br");
    REQUIRE(std::holds_alternative<element_node>(n));
    auto& e = std::get<element_node>(n);
    CHECK(e.tag == "br");
    CHECK(e.attrs.empty());
    CHECK(e.children.empty());
}

TEST_CASE("tag() preserves attribute order") {
    auto n = tag("div", {{"id", "1"}, {"class", "box"}, {"data-x", "y"}});
    auto& attrs = std::get<element_node>(n).attrs;
    REQUIRE(attrs.size() == 3);
    CHECK(attrs[0].first == "id");
    CHECK(attrs[1].first == "class");
    CHECK(attrs[2].first == "data-x");
}

TEST_CASE("tag() nests children") {
    auto n = tag("ul", {}, {
        tag("li", {}, {text("one")}),
        tag("li", {}, {text("two")})
    });
    auto& e = std::get<element_node>(n);
    CHECK(e.children.size() == 2);
    CHECK(std::get<element_node>(e.children[0]).tag == "li");
}
