#include <iostream>

#include "rencpp/ren.hpp"

using namespace ren;

int main(int, char **) {

    Block empty {};

    // This is a tricky case

    Block singleton = Block::construct("foo");
    Value singletonAsValue = singleton;

    static_cast<Block>(singletonAsValue);

    Block singletonInitializer = Block::construct(singleton);

    print(singletonInitializer);

    Block threeEmpties {Block {}, Block {}, Block {}};

    Block randomStuff {"blue", Block {true, 1020}, 3.04};
    print(randomStuff);
}
