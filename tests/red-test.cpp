// We only do this if we've built for Red

#include "rencpp/red.hpp"

using namespace red;

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("rebol test", "[rebol]")
{
    runtime.doMagicOnlyRedCanDo();
}
