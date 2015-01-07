#include <iostream>
#include <string>

#include "rencpp/ren.hpp"
#include "rencpp/runtime.hpp"

using namespace ren;

int main(int, char **) {

    Function printBlockString = makeFunction(
        "{Demonstration of the C++ Extension mechanism}"
        "blk [block!] {The block to print}",

        REN_STD_FUNCTION,

        [](Block const & blk) -> Logic {
            print("blk is", blk);
            return true;
        }
    );

    // Add extension to the environment.  For why we need the "quote", see
    // the answers here:
    //
    //    http://stackoverflow.com/questions/27641809/

    runtime("some-ext: quote", printBlockString);

    // Call the extension under its new name

    runtime("some-ext [1 2 3]");
}
