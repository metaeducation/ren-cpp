#include <iostream>

#include "rencpp/ren.hpp"

using namespace ren;

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("assign test", "[assign]")
{
    Integer someInt {10};
    Value someValue;

    someValue = someInt;

    Block someBlock {10, "foo"};

    Block someOtherBlock {20, "bar"};

    someBlock = someOtherBlock;
}
