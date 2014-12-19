#include <iostream>

#include "rencpp/ren.hpp"

using namespace ren;

int main(int, char **) {
    Integer someInt {10};
    Value someValue;

    someValue = someInt;

    Block someBlock {10, "foo"};

    Block someOtherBlock {20, "bar"};

    someBlock = someOtherBlock;
}
