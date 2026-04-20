#include <doctest.h>
#include "weasel/compiler/transpiler.hpp"

using weasel::compiler::collect_component_infos;

TEST_CASE("collect_component_infos: single component with position") {
    std::string src = "component alpha(int x) {}\n";
    auto v = collect_component_infos(src);
    REQUIRE(v.size() == 1);
    CHECK(v[0].name == "alpha");
    CHECK(v[0].decl_offset == 10);  // after "component "
    CHECK(v[0].decl_line == 1);
    CHECK(v[0].decl_column == 10);
    CHECK(v[0].params_raw_text == "int x");
}

TEST_CASE("collect_component_infos: multi-line source") {
    std::string src =
        "// header comment\n"
        "#include <x>\n"
        "\n"
        "component beta(\n"
        "    const foo_props& p\n"
        ") {\n"
        "    return <div/>;\n"
        "}\n";
    auto v = collect_component_infos(src);
    REQUIRE(v.size() == 1);
    CHECK(v[0].name == "beta");
    CHECK(v[0].decl_line == 4);
    CHECK(v[0].decl_column == 10);  // "component " is 10 chars
    CHECK(v[0].params_raw_text == "\n    const foo_props& p\n");
}

TEST_CASE("collect_component_infos: nested parens in params") {
    std::string src = "component gamma(std::function<int(int)> f, int y) {}\n";
    auto v = collect_component_infos(src);
    REQUIRE(v.size() == 1);
    CHECK(v[0].params_raw_text == "std::function<int(int)> f, int y");
}

TEST_CASE("collect_component_infos: string with paren inside") {
    std::string src = "component delta(const char* s = \")\") {}\n";
    auto v = collect_component_infos(src);
    REQUIRE(v.size() == 1);
    CHECK(v[0].params_raw_text == "const char* s = \")\"");
}

TEST_CASE("collect_component_infos: ignores 'component' in comments/strings") {
    std::string src =
        "// component foo(int) {}\n"
        "const char* s = \"component bar(int)\";\n"
        "component real(int) {}\n";
    auto v = collect_component_infos(src);
    REQUIRE(v.size() == 1);
    CHECK(v[0].name == "real");
}

TEST_CASE("collect_component_infos: ignores 'component' without following '('") {
    std::string src = "component notadecl;\ncomponent real(int){}\n";
    auto v = collect_component_infos(src);
    REQUIRE(v.size() == 1);
    CHECK(v[0].name == "real");
}

TEST_CASE("collect_component_infos: name set derivable from infos") {
    auto v = collect_component_infos("component alpha(int) {}\ncomponent beta(int){}\n");
    REQUIRE(v.size() == 2);
    CHECK(v[0].name == "alpha");
    CHECK(v[1].name == "beta");
}

TEST_CASE("collect_component_infos: multiple components") {
    std::string src =
        "component alpha(int) {}\n"
        "component beta(int) {}\n"
        "component gamma(int) {}\n";
    auto v = collect_component_infos(src);
    REQUIRE(v.size() == 3);
    CHECK(v[0].name == "alpha");
    CHECK(v[0].decl_line == 1);
    CHECK(v[1].name == "beta");
    CHECK(v[1].decl_line == 2);
    CHECK(v[2].name == "gamma");
    CHECK(v[2].decl_line == 3);
}
