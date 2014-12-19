#include <iostream>
#include <cassert>

#include "rencpp/ren.hpp"

using namespace ren;

int main(int, char **) {

    Value someIntAsValue = 10;
    assert(someIntAsValue.isInteger());

    // should not throw an exception
    Integer someInt = static_cast<Integer>(someIntAsValue);

    std::cout << "SUCCESS: integer cast!\n";

    Value someIntConstValue = someIntAsValue;
    Integer anotherInt = static_cast<Integer>(someIntConstValue);

    try {
        // You can't treat the bits of an Integer as a Float in the Rebol or
        // Red world...C++ *could* have a cast operator for it.  See notes:
        //
        //     https://github.com/hostilefork/rencpp/issues/7

        Float illegalFloat = static_cast<Float>(someIntAsValue);

        // that should have thrown an exception...shouldn't get here...

        throw std::runtime_error(
            "FAILURE: int shouldn't cast from float in this model"
        );
    }
    catch (std::bad_cast const & e) {

        std::cout << "SUCCESS: cast int to float threw bad_cast exception!\n";

    }
}
