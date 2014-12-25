#include <iostream>
#include <string>

#include "rencpp/ren.hpp"
#include "rencpp/runtime.hpp"

using namespace ren;

int main(int, char **) {

    auto printBlockString = make_Extension(
        "{Demonstration of the C++ Extension mechanism}"
        "blk [block!] {The block to print}"
        "str [string!] {The string to print}",

        [](Block && blk, String && str) -> Logic {
            print("blk is", blk);
            print("str is", str);
            return true;
        }
    );

    auto aBlock = Block {"print", "{Hello}"};
    auto aString = String {"{hi there}"};

    print("Before the call block is", aBlock, "and string is", aString);

    for (int index = 0; index < 2; index++) {
        if (printBlockString(aBlock, aString))
            print("EXTENSION RETURNED TRUE!");
        else
            print("EXTENSION RETURNED FALSE!");
    }

    // Add extension to the environment.  It shouldn't require the proxy;
    // seems a Rebol bug...function *values* should be inert:
    //
    //    http://stackoverflow.com/questions/27641809/
    //
    // So you *should* be able to just write:
    //
    //    SetWord {"some-ext:"}(someExt);

    runtime("some-ext: first", Block {printBlockString});

    // Call the extension under its new name

    runtime("some-ext [1 2 3] {foo}");
}
