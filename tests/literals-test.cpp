#include <iostream>
#include <string>
#include <cassert>

#include "rencpp/ren.hpp"

using namespace ren;

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("literals construction", "[literals]")
{
    SECTION("default")
    {
        Value value;
        REQUIRE(value.isUnset());
    }

    SECTION("unset")
    {
        Value value = unset;
        REQUIRE(value.isUnset());
    }

    SECTION("logic")
    {
        Value value = false;
        REQUIRE(value.isLogic());
    }

    SECTION("integer")
    {
        Value value = 1;
        REQUIRE(value.isInteger());
    }


    SECTION("float construction")
    {
        Value value = 10.20;
        REQUIRE(value.isFloat());
    }
}
