#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <sstream>
#include <string>
#include "weasel/compiler/transpiler.hpp"

using weasel::compiler::collect_component_infos;
using weasel::compiler::transpile;

static std::string run(std::string_view src) {
    std::ostringstream oss;
    transpile(src, oss);
    return oss.str();
}

TEST_CASE("passthrough: no weasel syntax gets preamble prepended") {
    std::string src =
        "#include <vector>\n"
        "// a comment with <angle> brackets\n"
        "int main() {\n"
        "    int a = 1, b = 2;\n"
        "    if (a < b) { return 0; }\n"
        "    std::string s = \"<div>not ccx</div>\";\n"
        "    return 0;\n"
        "}\n";
    const std::string preamble = "#include \"weasel/weasel.hpp\"\n";
    CHECK(run(src) == preamble + src);
}

TEST_CASE("component keyword rewrites to weasel::node") {
    std::string src = "component foo(const foo_props& p) { return nullptr; }\n";
    std::string out = run(src);
    CHECK(out.find("weasel::node foo(") != std::string::npos);
}

TEST_CASE("collect_component_infos picks up declarations") {
    auto v = collect_component_infos("component alpha(int) {}\ncomponent beta(int){}\n");
    REQUIRE(v.size() == 2);
    CHECK(v[0].name == "alpha");
    CHECK(v[1].name == "beta");
}

TEST_CASE("self-closing html tag") {
    std::string src = "node f() { return <br/>; }\n";
    std::string out = run(src);
    CHECK(out.find("weasel::tag(\"br\")") != std::string::npos);
}

TEST_CASE("html tag with literal and expression attrs") {
    std::string src = "node f(const std::string& cls) { return <span class={cls} id=\"x\">hi</span>; }\n";
    std::string out = run(src);
    CHECK(out.find("weasel::tag(\"span\"") != std::string::npos);
    CHECK(out.find("{\"class\", (cls)}") != std::string::npos);
    CHECK(out.find("{\"id\", \"x\"}") != std::string::npos);
    CHECK(out.find("weasel::text(\"hi\")") != std::string::npos);
}

TEST_CASE("expression child wraps with text()") {
    std::string src = "node f(const std::string& s) { return <span>{s}</span>; }\n";
    std::string out = run(src);
    CHECK(out.find("weasel::text(s)") != std::string::npos);
}

TEST_CASE("component element emits designated init call") {
    std::string src =
        "component badge(const badge_props& p) { return <span>{p.label}</span>; }\n"
        "node f() { return <badge label=\"x\" />; }\n";
    std::string out = run(src);
    CHECK(out.find("badge({.label = \"x\"})") != std::string::npos);
}

TEST_CASE("if child lowered to IIFE") {
    std::string src = "node f(bool show) { return <div>{ if (show) { <p>yes</p> } }</div>; }\n";
    std::string out = run(src);
    CHECK(out.find("[&]() -> weasel::node") != std::string::npos);
    CHECK(out.find("if (show) { return weasel::tag(\"p\"") != std::string::npos);
}

TEST_CASE("for child lowered to fragment-building IIFE") {
    std::string src =
        "node f(const std::vector<int>& xs) {\n"
        "    return <ul>{ for (int x : xs) { <li>{x}</li> } }</ul>;\n"
        "}\n";
    std::string out = run(src);
    CHECK(out.find("weasel::node_list weasel_nodes_") != std::string::npos);
    CHECK(out.find("for (int x : xs)") != std::string::npos);
    CHECK(out.find("weasel::fragment(std::move(weasel_nodes_))") != std::string::npos);
}

TEST_CASE("if-else child") {
    std::string src = "node f(bool b) { return <div>{ if (b) { <a>y</a> } else { <a>n</a> } }</div>; }\n";
    std::string out = run(src);
    CHECK(out.find("else {") != std::string::npos);
}

TEST_CASE("control characters in text node are hex-escaped") {
    // \x01 (SOH) must not appear raw inside the generated C++ string literal.
    std::string src = "node f() { return <p>hello\x01world</p>; }\n";
    std::string out = run(src);
    CHECK(out.find("\\x01") != std::string::npos);
    CHECK(out.find('\x01') == std::string::npos);
}

TEST_CASE("embedded double-quote in text node is escaped") {
    std::string src = "node f() { return <p>say \"hi\"</p>; }\n";
    std::string out = run(src);
    CHECK(out.find("\\\"hi\\\"") != std::string::npos);
}
