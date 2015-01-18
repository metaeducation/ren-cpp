#include <iostream>

#include "rencpp/ren.hpp"

using namespace ren;

#include "catch.hpp"

TEST_CASE("assign test", "[rebol]")
{
    Integer someInt {10};
    Value someValue;

    someValue = someInt;

    Block someBlock {10, "foo"};

    Block someOtherBlock {20, "bar"};

    someBlock = someOtherBlock;

    CHECK(someBlock.isEqualTo(someOtherBlock));
}
