#include "doctest.h"
#include "weasel/context.hpp"
#include <thread>
#include <string>

using namespace weasel;

struct theme { std::string name; };
using theme_ctx = context<theme>;

TEST_CASE("current() throws when no provider") {
    CHECK_THROWS_AS(theme_ctx::current(), std::runtime_error);
}

TEST_CASE("provider makes value available") {
    theme t{"dark"};
    theme_ctx::provider p(t);
    CHECK(theme_ctx::current().name == "dark");
}

TEST_CASE("provider pops on scope exit") {
    {
        theme t{"light"};
        theme_ctx::provider p(t);
        CHECK(theme_ctx::current().name == "light");
    }
    CHECK_THROWS_AS(theme_ctx::current(), std::runtime_error);
}

TEST_CASE("nested providers shadow outer") {
    theme outer{"outer"};
    theme_ctx::provider p1(outer);
    CHECK(theme_ctx::current().name == "outer");
    {
        theme inner{"inner"};
        theme_ctx::provider p2(inner);
        CHECK(theme_ctx::current().name == "inner");
    }
    CHECK(theme_ctx::current().name == "outer");
}

TEST_CASE("thread-local isolation") {
    theme main_theme{"main"};
    theme_ctx::provider p(main_theme);

    std::string thread_result;
    std::thread t([&] {
        // No provider in this thread — should throw
        try {
            (void)theme_ctx::current();
            thread_result = "no_throw";
        } catch (const std::runtime_error&) {
            thread_result = "threw";
        }
    });
    t.join();

    CHECK(thread_result == "threw");
    CHECK(theme_ctx::current().name == "main");
}
