#include <iostream>
#include <string>
#include <cassert>

#include "rencpp/ren.hpp"

using namespace ren;

//
// This shows a "less C++ and more Ren" variant of how you might use the
// binding.  All we've pulled out is the variableName; the printing is
// done from inside of Ren.
//

int main(int, char **) {
    auto variable = Word {"foo"};

    optional<AnyValue> result = runtime(
        "data: {Hello [Ren C++ Binding] World!}"

        "rule:", Block {
            "thru {[}"
            "copy", variable, "to {]}"
            "to end"
        },

        "either result: parse data rule", Block {
            "print", Block {"{Success and target was}", variable},
        }, "["
            "print {PARSE failed.}"
        "]",
        "result"
    );
}

//
// (Note: Although runs of characters in a block construction may not break
// up a pairing of delimiters, if C++ sees a string end on one line and get
// picked up on the next it will merge the character data.)
//
