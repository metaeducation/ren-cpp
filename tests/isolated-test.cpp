// This file is here for throwing in temporary one-off tests
// that are not meant to be added permanently to the test suite.
//
// If there are any tests in this file with this tag, then 
// Travis will run it in isolation from the other tests.

#include "rencpp/ren.hpp"

using namespace ren;

#include "catch.hpp"

TEST_CASE("isolated test", "[isolated]")
{
    SetWord sw ("foo");
    runtime(sw, "{Hello}");
}
