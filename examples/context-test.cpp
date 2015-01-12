#include <iostream>
#include <cassert>
#include <memory>

#include "rencpp/ren.hpp"

using namespace ren;

int main(int, char **) {
    // returning a local by reference from the setFinder upsets Clang, so we
    // heap allocate

    std::unique_ptr<Context> contextOne {new Context};
    std::unique_ptr<Context> contextTwo {new Context};

    // test of making which runtime is used in creates a
    // property of some global factor...
   
    int contextNumber = 1;

    Context::setFinder(
        [&](Engine *) -> Context & {
            if (contextNumber == 1)
                return *contextOne;
            if (contextNumber == 2)
                return *contextTwo;
            throw std::runtime_error("Invalid context number");
        }
    );
    
    // make a set-word for x, then "apply" it to 10
    // we are creating this in contextOne
    SetWord {"x"}(10);

    // now print using runtime apply notation
    assert(runtime("integer? x"));

    // switch the runtime that will be found by the next call now...
    contextNumber = 2;

    // we see x is not in this one; don't use individual pieces
    assert(runtime("unset? get/any 'x"));

    // now using the default let's set x in the second runtime...
    SetWord {"x"}(20);
    assert(runtime("integer? x"));

    // even though our default is to run in the second runtime
    // at the moment, let's override it using an additional parameter
    // to the constructor

    auto y = SetWord {"y", contextOne.get()};
    y(30);

    // Switch active contexts and see that we set y
    contextNumber = 1;
    assert(runtime("integer? get/any 'y"));
    assert((*contextOne)("integer? get/any 'y"));
}
