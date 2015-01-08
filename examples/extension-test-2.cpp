#include <iostream>
#include <string>

#include "rencpp/ren.hpp"

using namespace ren;

class Adder {
private:
    int amount;
public:
    Adder(int amount) : amount (amount) {}

    Integer operator()(Integer const & value) {
        return value + amount;
    }
};

int main(int, char **) {

    auto addFive = makeFunction(
        "{Demonstration of using an operator() overloaded class}"
        "value [integer!]",

        REN_STD_FUNCTION,

        // This won't work, operator() may (in the general case) be a template
        // http://stackoverflow.com/a/8670836/211160
        /* Adder {5} */

        // This works, but makes you repeat the type signature.  That's life.
        std::function<Integer(Integer const &)> {Adder {5}}
    );

    // Here we actually are using the behavior being complained about here
    // affecting assignment, by just using the function inline without any
    // call to an APPLY:
    //
    //     http://stackoverflow.com/questions/27641809/
    //

    assert(static_cast<Integer>(runtime("10 +", addFive, 100)) == 115);
}
