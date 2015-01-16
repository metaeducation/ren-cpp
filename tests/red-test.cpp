// We only do this if we've built for Red

#include "rencpp/red.hpp"

using namespace red;

#include "catch.hpp"

TEST_CASE("red test", "[red]")
{
    runtime.doMagicOnlyRedCanDo();
}
