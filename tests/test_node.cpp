#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "weasel/node.hpp"

using namespace weasel;

TEST_CASE("text_node holds string") {
    node n = text_node{"hello"};
    REQUIRE(std::holds_alternative<text_node>(n));
    CHECK(std::get<text_node>(n).content == "hello");
}

TEST_CASE("raw_node holds html") {
    node n = raw_node{"<b>bold</b>"};
    REQUIRE(std::holds_alternative<raw_node>(n));
    CHECK(std::get<raw_node>(n).html == "<b>bold</b>");
}

TEST_CASE("element_node with attrs and children") {
    element_node e;
    e.tag = "div";
    e.attrs = {{"class", "box"}};
    e.children.push_back(text_node{"hi"});

    node n = e;
    REQUIRE(std::holds_alternative<element_node>(n));
    auto& elem = std::get<element_node>(n);
    CHECK(elem.tag == "div");
    CHECK(elem.attrs.size() == 1);
    CHECK(elem.children.size() == 1);
}

TEST_CASE("fragment_node holds children") {
    fragment_node f;
    f.children.push_back(text_node{"a"});
    f.children.push_back(text_node{"b"});

    node n = f;
    REQUIRE(std::holds_alternative<fragment_node>(n));
    CHECK(std::get<fragment_node>(n).children.size() == 2);
}

TEST_CASE("monostate is default-constructible") {
    node n;
    CHECK(std::holds_alternative<std::monostate>(n));
}

TEST_CASE("node is movable") {
    node a = text_node{"move me"};
    node b = std::move(a);
    REQUIRE(std::holds_alternative<text_node>(b));
    CHECK(std::get<text_node>(b).content == "move me");
}

TEST_CASE("nested nodes") {
    node inner = text_node{"inner"};
    element_node outer;
    outer.tag = "span";
    outer.children.push_back(std::move(inner));

    node n = std::move(outer);
    auto& e = std::get<element_node>(n);
    CHECK(std::get<text_node>(e.children[0]).content == "inner");
}
