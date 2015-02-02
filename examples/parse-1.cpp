#include <iostream>
#include <string>
#include <cassert>

#include "rencpp/ren.hpp"

using namespace ren;

//
// This shows a "more C++ and less Ren" variant of how you might use the
// binding.  Instead of being mostly Ren text with objects spliced in, it
// is mostly objects with text runs spliced in.
//

int main(int, char **) {
    std::string data {"Hello [Ren C++ Binding] World!"};

    Word variable {"foo"};

    Block rule {
        "thru {[}",
        "copy", variable, "to {]}",
        "to end"
    };

    auto result = runtime("parse", data, rule);

    if (result)
        std::cout << "Success and target was " << variable() << "\n";
    else
        std::cout << "PARSE failed.";
}
