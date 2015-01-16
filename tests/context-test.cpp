#include <iostream>
#include <cassert>
#include <memory>

#include "rencpp/ren.hpp"

using namespace ren;

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("context test", "[context]")
{
    // returning a local by reference from the setFinder upsets Clang, so we
    // heap allocate

    Context contextOne {};
    Context contextTwo {};

    // test of making which runtime is used in creates a
    // property of some global factor...
   
    int contextNumber = 1;

    Context::setFinder(
        [&](Engine *) -> Context & {
            if (contextNumber == 1)
                return contextOne;
            if (contextNumber == 2)
                return contextTwo;
            throw std::runtime_error("Invalid context number");
        }
    );
    
    // make a set-word for x, then "apply" it to 10
    // we are creating this in contextOne
    SetWord {"x"}(10);

    // now print using runtime apply notation
    REQUIRE(runtime("integer? x"));

    // switch the runtime that will be found by the next call now...
    contextNumber = 2;

    // we see x is not in this one; don't use individual pieces
    REQUIRE(runtime("unset? get/any 'x"));

    // now using the default let's set x in the second runtime...
    SetWord {"x"}(20);
    REQUIRE(runtime("integer? x"));

    // even though our default is to run in the second runtime
    // at the moment, let's override it using an additional parameter
    // to the constructor

    auto y = SetWord {"y", contextOne};
    print(runtime("bind? quote", y));
    y(30);

    // Switch active contexts and see that we set y
    contextNumber = 1;
    REQUIRE(runtime("integer? get/any 'y"));

    // This test currently not working, more context work needed
    /* REQUIRE(contextOne("integer? get/any 'y")); */
}
