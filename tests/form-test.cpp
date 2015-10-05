#include <iostream>
#include <string>
#include <cassert>

#include "rencpp/ren.hpp"

using namespace ren;

#include "catch.hpp"

TEST_CASE("form test", "[rebol] [form]")
{
    CHECK(to_string(AnyValue {10}) == "10");
    CHECK(to_string(AnyValue {1.5}) == "1.5");
    CHECK(to_string(AnyValue {true}) == "true");

    // The only type willing to implicitly cast to a std::string is
    // ren::String; all others must be explicit with to_string which
    // performs equivalently to TO-STRING (new behavior, formerly FORM)

    std::string converted = String {"Hello World"};
    CHECK(converted == "Hello World");

    CHECK(String {"Hello World"}.isEqualTo("Hello World"));

    // one Unicode test is better than zero unicode tests :-)

    CHECK(String {"\n\t\u0444"}.isEqualTo("\n\t\u0444"));
    CHECK(String {"^/^-^(0444)"}.isEqualTo("\n\t\u0444"));
}
