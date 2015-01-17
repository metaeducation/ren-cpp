#include <iostream>
#include <cassert>
#include <memory>

#include "rencpp/ren.hpp"

using namespace ren;

#include "catch.hpp"

TEST_CASE("context test", "[context]")
{
    Context defaultContext = Context::current();

    Context contextOne {};
    Context contextTwo {};

    // We install a new "context finder" which uses an integer to indicate
    // which context is currently in effect.
   
    int contextNumber = 1;

    auto oldFinder = Context::setFinder(
        [&](Engine *) -> Context {
            if (contextNumber == 1)
                return contextOne;
            if (contextNumber == 2)
                return contextTwo;
            throw std::runtime_error("Invalid context number");
        }
    );
    
    // Use function apply notation on a SetWord to set x to 10

    SetWord set_x {"x"};
    set_x(10);

    // now test it using runtime apply notation

    CHECK(runtime("x = 10"));

    // change the global state, so the runtime is operating in contextTwo
    // if no override provided.

    contextNumber = 2;

    // Here in context 2, the changes to x had no effect; x is unset

    CHECK(runtime("unset? get/any 'x"));

    // Let's use function apply notation to set x in contextTwo, this time
    // using a temporary SetWord...

    SetWord {"x"}(20);

    // Require that to have worked...

    CHECK(runtime("x = 20"));

    // even though our default is to run in the second context
    // at the moment, let's override it using an additional parameter
    // to the constructor...and write a new value into contextOne...this
    // time using the `auto` keyword to put the type on the right hand side

    auto y = SetWord {"y", contextOne};
    y(30);

    // If we apply on a context, it will effectively run a bind/copy on
    // the argument so that it picks up bindings in that context.  Be aware
    // it makes a deep copy of the argument to do so.

    CHECK(contextOne("y = 30"));

    // Now switch active contexts

    contextNumber = 1;

    // We notice that an unparameterized call now finds y in contextOne

    CHECK(runtime("y = 30"));

    // Restore the context finder to the previous one (default) so other
    // tests will work correctly

    Context::setFinder(oldFinder);
}
