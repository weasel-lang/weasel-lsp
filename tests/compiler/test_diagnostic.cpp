#include <doctest.h>
#include <sstream>
#include "weasel/compiler/diagnostic.hpp"
#include "weasel/compiler/transpiler.hpp"

using weasel::compiler::diagnostic;
using weasel::compiler::parse_error;
using weasel::compiler::severity;
using weasel::compiler::transpile;

static void run_expecting_parse_error(std::string_view src, diagnostic& out) {
    std::ostringstream oss;
    try {
        transpile(src, oss);
        FAIL("expected parse_error");
    } catch (const parse_error& e) {
        out = e.diag;
    }
}

TEST_CASE("parse_error: unmatched closing tag carries byte span") {
    diagnostic d;
    run_expecting_parse_error("node f() { return <div></span>; }\n", d);
    CHECK(d.sev == severity::error);
    CHECK(d.code == "weasel.parse");
    CHECK_FALSE(d.message.empty());
    // The span should point somewhere inside the source.
    CHECK(d.span.begin > 0);
    CHECK(d.span.begin <= 40);
}

TEST_CASE("parse_error: EOF inside tag carries span") {
    diagnostic d;
    run_expecting_parse_error("node f() { return <div\n", d);
    CHECK(d.span.begin > 0);
}

TEST_CASE("parse_error: bad attr value") {
    diagnostic d;
    run_expecting_parse_error("node f() { return <div class=5/>; }\n", d);
    CHECK(d.message.find("'\"' or '{'") != std::string::npos);
}
