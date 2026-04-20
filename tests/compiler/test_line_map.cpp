#include <doctest.h>

#include <sstream>
#include "weasel/compiler/transpiler.hpp"

using namespace weasel::compiler;

TEST_CASE("transpile_with_map — empty input produces ok result") {
    std::ostringstream out;
    auto r = transpile_with_map("", out);
    CHECK(r.ok);
    CHECK(r.diagnostics.empty());
    CHECK(r.components.empty());
}

TEST_CASE("transpile_with_map — pure C++ passthrough has one cpp span") {
    std::ostringstream out;
    auto r = transpile_with_map("int f() { return 0; }\n", out);
    REQUIRE(r.ok);
    REQUIRE(r.line_map.size() == 1);
    CHECK(r.line_map[0].kind == span_kind::cpp_passthrough);
    CHECK(r.line_map[0].cc_line_begin == 1);
}

TEST_CASE("transpile_with_map — single CCX expression produces cpp + ccx spans") {
    std::ostringstream out;
    auto r = transpile_with_map("node f() { return <div/>; }\n", out);
    REQUIRE(r.ok);
    // At minimum, there should be a ccx_region span covering the <div/>.
    bool saw_ccx = false;
    for (const auto& sp : r.line_map) {
        if (sp.kind == span_kind::ccx_region)
            saw_ccx = true;
    }
    CHECK(saw_ccx);
}

TEST_CASE("transpile_with_map — parse error becomes diagnostic (ok=false)") {
    std::ostringstream out;
    auto r = transpile_with_map("node f() { return <div></span>; }\n", out);
    CHECK_FALSE(r.ok);
    REQUIRE(r.diagnostics.size() >= 1);
    CHECK(r.diagnostics[0].sev == severity::error);
    CHECK(r.diagnostics[0].code == "weasel.parse");
}

TEST_CASE("transpile_with_map — collects components with positions") {
    std::ostringstream out;
    auto r = transpile_with_map(
        "component MyCard(int) { return <div/>; }\n"
        "node page() { return <MyCard/>; }\n",
        out);
    REQUIRE(r.ok);
    REQUIRE(r.components.size() == 1);
    CHECK(r.components[0].name == "MyCard");
    CHECK(r.components[0].decl_line == 1);
}

TEST_CASE("transpile_with_map — if inside CCX with multi-line body") {
    // The outer <div> is the CCX region; the if-chain spans multiple weasel
    // lines inside it. Verify the transpiler succeeds, produces ccx+cpp spans,
    // and emits the if-chain IIFE.
    const char* src =
        "node f(bool b) {\n"        // line 1
        "return <div>{ if (b) {\n"  // line 2  — CCX starts at <div>
        "    <p>yes</p>\n"          // line 3
        "} }</div>;\n"              // line 4  — CCX ends at </div>
        "}\n";                      // line 5
    std::ostringstream out;
    auto r = transpile_with_map(src, out);
    REQUIRE(r.ok);
    bool saw_ccx = false, saw_cpp = false;
    for (const auto& sp : r.line_map) {
        if (sp.kind == span_kind::ccx_region) {
            saw_ccx = true;
            // The CCX region spans at least lines 2–4 of the .weasel source.
            CHECK(sp.weasel_line_end > sp.weasel_line_begin);
        }
        if (sp.kind == span_kind::cpp_passthrough)
            saw_cpp = true;
    }
    CHECK(saw_ccx);
    CHECK(saw_cpp);
    CHECK(out.str().find("[&]() -> weasel::node") != std::string::npos);
}

TEST_CASE("transpile_with_map — for head spanning two source lines") {
    const char* src =
        "node f(const std::vector<int>& v) {\n"      // line 1
        "return <ul>{ for (\n"                       // line 2  — CCX starts
        "    int x : v) { <li>{x}</li> } }</ul>;\n"  // line 3
        "}\n";                                       // line 4
    std::ostringstream out;
    auto r = transpile_with_map(src, out);
    REQUIRE(r.ok);
    bool saw_multi_line_ccx = false;
    for (const auto& sp : r.line_map) {
        if (sp.kind == span_kind::ccx_region && sp.weasel_line_end > sp.weasel_line_begin)
            saw_multi_line_ccx = true;
    }
    CHECK(saw_multi_line_ccx);
}

TEST_CASE("transpile_with_map — empty CCX region (self-closing on own line)") {
    // A single-line self-closing element is a valid CCX region with begin==end.
    const char* src =
        "node f() {\n"     // line 1
        "return <br/>;\n"  // line 2
        "}\n";             // line 3
    std::ostringstream out;
    auto r = transpile_with_map(src, out);
    REQUIRE(r.ok);
    bool saw_ccx = false;
    for (const auto& sp : r.line_map) {
        if (sp.kind == span_kind::ccx_region)
            saw_ccx = true;
    }
    CHECK(saw_ccx);
    // Verify the generated output contains the weasel br call.
    CHECK(out.str().find("weasel::tag(\"br\")") != std::string::npos);
}

TEST_CASE("cc_line_to_weasel — binary search over line_map") {
    std::vector<line_span> map = {
        {1, 2, 1, 2, span_kind::cpp_passthrough},
        {3, 5, 3, 5, span_kind::ccx_region},
        {6, 7, 6, 7, span_kind::cpp_passthrough},
    };
    SUBCASE("cpp_passthrough preserves line offset") {
        auto r = cc_line_to_weasel(map, 2);
        CHECK(r.weasel_line == 2);
        REQUIRE(r.span != nullptr);
        CHECK(r.span->kind == span_kind::cpp_passthrough);
    }
    SUBCASE("ccx_region collapses to span start") {
        auto r = cc_line_to_weasel(map, 4);
        CHECK(r.weasel_line == 3);
        REQUIRE(r.span != nullptr);
        CHECK(r.span->kind == span_kind::ccx_region);
    }
    SUBCASE("trailing cpp_passthrough after a ccx") {
        auto r = cc_line_to_weasel(map, 7);
        CHECK(r.weasel_line == 7);
    }
    SUBCASE("line past EOF returns not-found") {
        auto r = cc_line_to_weasel(map, 100);
        CHECK(r.span == nullptr);
        CHECK(r.weasel_line == 0);
    }
}
