#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include "weasel/compiler/transpiler.hpp"
#include <sstream>
#include <string>

using weasel::compiler::transpile;
using weasel::compiler::collect_components;

static std::string run(std::string_view src) {
    std::ostringstream oss;
    transpile(src, oss);
    return oss.str();
}

TEST_CASE("passthrough: no weasel syntax = identical output") {
    std::string src =
        "#include <vector>\n"
        "// a comment with <angle> brackets\n"
        "int main() {\n"
        "    int a = 1, b = 2;\n"
        "    if (a < b) { return 0; }\n"
        "    std::string s = \"<div>not ccx</div>\";\n"
        "    return 0;\n"
        "}\n";
    CHECK(run(src) == src);
}

TEST_CASE("component keyword rewrites to weasel::node") {
    std::string src = "component foo(const foo_props& p) { return nullptr; }\n";
    std::string out = run(src);
    CHECK(out.find("weasel::node foo(") != std::string::npos);
}

TEST_CASE("collect_components picks up declarations") {
    auto set = collect_components("component alpha(int) {}\ncomponent beta(int){}\n");
    CHECK(set.count("alpha") == 1);
    CHECK(set.count("beta") == 1);
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
    std::string src =
        "node f(bool show) { return <div>{ if (show) { <p>yes</p> } }</div>; }\n";
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
    CHECK(out.find("weasel::node_list __w") != std::string::npos);
    CHECK(out.find("for (int x : xs)") != std::string::npos);
    CHECK(out.find("weasel::fragment(std::move(__w))") != std::string::npos);
}

TEST_CASE("if-else child") {
    std::string src = "node f(bool b) { return <div>{ if (b) { <a>y</a> } else { <a>n</a> } }</div>; }\n";
    std::string out = run(src);
    CHECK(out.find("else {") != std::string::npos);
}
