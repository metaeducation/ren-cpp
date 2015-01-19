// This file is here for throwing in temporary one-off tests
// that are not meant to be added permanently to the test suite.
//
// If there are any tests in this file with this tag, then 
// Travis will run it in isolation from the other tests.
//
// Put the test with the appropriate runtime to prevent it from
// causing unnecessary failures in the runtime you're not testing

#include "rencpp/ren.hpp"

using namespace ren;

#include "catch.hpp"

TEST_CASE("isolated test", "[isolated] [rebol]")
{
}


TEST_CASE("isolated test", "[isolated] [red]")
{
}
