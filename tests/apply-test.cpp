#include <iostream>
#include <cassert>

#include "rencpp/ren.hpp"

using namespace ren;

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("apply test", "[apply]")
{
    SECTION("set-word success")
    {
        Value result = SetWord {"w"}(10);
        REQUIRE(result.isInteger());
        REQUIRE(static_cast<int>(static_cast<Integer>(result)) == 10);
    }

    SECTION("set-word failure")
    {
        bool caught = false;
        try {
            SetWord {"w"}(10, 20);
        }
        catch (evaluation_error const & e) {
            caught = true;
        }

        REQUIRE(caught);
    }

    SECTION("none failure")
    {
        bool caught = false;
        try {
            // technical note: explicit none(arg1, arg2...) is now illegal
            Value value = none;
            value.apply(10);
        }
        catch (evaluation_error const & e) {
            caught = true;
        }

        REQUIRE(caught);
    }
}
