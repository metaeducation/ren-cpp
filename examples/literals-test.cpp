#include <iostream>
#include <string>
#include <cassert>

#include "rencpp/ren.hpp"

using namespace ren;

static void dump(std::string name, Value value) {
    static auto type_of = Word ("type?");

    std::cout << name << " is of type " << type_of(value)
        << " and has value " << value << "\n";
}


int main(int, char **) {

    // UNSET!

    Value valueDefaultConstruct;
    dump("valueDefaultConstruct", valueDefaultConstruct);
    assert(valueDefaultConstruct.isUnset());


    // LOGIC!

    Value valueFalse = false;
    dump("valueFalse", valueFalse);
    assert(valueFalse.isLogic());


    // INTEGER!

    Value valueOne = 1;
    dump("valueOne", valueOne);
    assert(valueOne.isInteger());


    // FLOAT!

    Value value10dot20 = 10.20;
    dump("value10dot20", value10dot20);
    assert(value10dot20.isFloat());

    
    // So...other possibilities for accepted types to make value
    // from the C++ world?
}
