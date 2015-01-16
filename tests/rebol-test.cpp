// We only do this if we've built for Rebol

#include "rencpp/rebol.hpp"

using namespace rebol;

#include "catch.hpp"

TEST_CASE("rebol test", "[rebol]")
{
    runtime.doMagicOnlyRebolCanDo();
}
