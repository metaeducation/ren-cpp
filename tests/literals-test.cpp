#include <iostream>
#include <string>
#include <cassert>

#include "rencpp/ren.hpp"

using namespace ren;

#include "catch.hpp"

TEST_CASE("literals construction", "[rebol]")
{
    SECTION("default")
    {
        Value value;
        CHECK(value.isUnset());
    }

    SECTION("unset")
    {
        Value value = unset;
        CHECK(value.isUnset());
    }

    SECTION("logic")
    {
        Value value = false;
        CHECK(value.isLogic());
    }

    SECTION("integer")
    {
        Value value = 1;
        CHECK(value.isInteger());
    }


    SECTION("float construction")
    {
        Value value = 10.20;
        CHECK(value.isFloat());
    }
}
