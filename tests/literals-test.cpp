#include <iostream>
#include <string>
#include <cassert>

#include "rencpp/ren.hpp"

using namespace ren;

#include "catch.hpp"

TEST_CASE("literals construction", "[rebol] [literals]")
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

        // https://github.com/hostilefork/rencpp/issues/24
        // No way ATM to test the "shouldn't compile" cases

        auto logical = [](Logic const &) {};

        logical(true); // should work
        // logical("hello"); // shouldn't compile (!)
        // logical(15); // shouldn't compile (!)

        logical(Logic {true}); // should work
        // logical(Logic {"hello"}); // it's o.k. this doesn't compile
        // logical(Logic {15}); // it's o.k. this doesn't compile
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
