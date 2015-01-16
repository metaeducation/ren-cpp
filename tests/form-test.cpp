#include <iostream>
#include <string>
#include <cassert>

#include "rencpp/ren.hpp"

using namespace ren;

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("form test", "[form]")
{
    REQUIRE(to_string(Value {10}) == "10");
    REQUIRE(to_string(Value {1.5}) == "1.5");
    REQUIRE(to_string(Value {true}) == "true");

    // The only type willing to implicitly cast to a std::string is
    // ren::String; all others must be explicit with to_string which
    // performs equivalently to TO-STRING (new behavior, formerly FORM)

    std::string converted = String {"Hello World"};
    REQUIRE(converted == "Hello World");

    REQUIRE(String {"Hello World"}.isEqualTo("Hello World"));

    // one Unicode test is better than zero unicode tests :-)

    REQUIRE(String {"\n\t\u0444"}.isEqualTo("\n\t\u0444"));
    REQUIRE(String {"^/^-^(0444)"}.isEqualTo("\n\t\u0444"));
}
