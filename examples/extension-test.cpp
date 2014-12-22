#include <iostream>
#include <string>

#include "rencpp/ren.hpp"
#include "rencpp/runtime.hpp"

using namespace ren;

int main(int, char **) {

    auto someExt = make_Extension(
        "{Demonstration of the C++ Extension mechanism}"
        "blk [block!] {The block to print}"
        "str [string!] {The string to print}",

        [](Block & blk, String & str) -> Logic {
            print("EXTENSION CALLED!\n");
            print("blk is", blk);
            print("str is", str);
            return true;
        }
    );

    auto aBlock = Block {"print", "{Hello}"};
    auto aString = String {"{hi there}"};

    print("Before the call block is", aBlock, "and string is", aString);

    for (int index = 0; index < 2; index++) {
        if (someExt(aBlock, aString))
            print("EXTENSION RETURNED TRUE!");
        else
            print("EXTENSION RETURNED FALSE!");
    }

    // Add extension to the environment
    SetWord {"some-ext:"}(someExt);

    // Call the extension under its new name
    runtime("some-ext [1 2 3] {foo}");

}
