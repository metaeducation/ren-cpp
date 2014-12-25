#include <iostream>
#include <string>

#include "rencpp/ren.hpp"
#include "rencpp/runtime.hpp"

using namespace ren;

class Adder {
private:
    int amount;
public:
    Adder(int amount) : amount (amount) {}

    Integer operator()(Integer && value) {
        return value + amount;
    }
};

int main(int, char **) {

    auto addFive = make_Extension(
        "{Demonstration of using an operator() overloaded class}"
        "value [integer!]",

        /* Adder(5) */ // won't work :-(
        // http://stackoverflow.com/a/8670836/211160
        std::function<Integer(Integer &&)>{Adder {5}} // will work...
    );

    assert(static_cast<Integer>(runtime("10 +", addFive, 100)) == 115);
}
