#include <iostream>
#include <cassert>

#include "rencpp/ren.hpp"
#include "rencpp/runtime.hpp"

using namespace ren;

int main(int, char **) {
    Value result;

    result = SetWord {"w:"}(10);
    assert(result.isInteger());

    try {
        SetWord {"w:"}(10, 20);
    }
    catch (too_many_args const & e) {
        print("2 is too many args to a generalized apply for set word!");
    }

    result = none;
    assert(result.isNone());

    try {
        // technical note: explicit none(arg1, arg2...) is now illegal
        result(10);
    }
    catch (too_many_args const & e) {
        print("generalized apply for none cannot have any arguments");
    }

}
