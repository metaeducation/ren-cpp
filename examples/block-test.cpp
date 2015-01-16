#include <iostream>

#include "rencpp/ren.hpp"

using namespace ren;

int main(int, char **) {

    Block empty {};

    // This is a tricky case

    Block singleton {"foo"};
    Value singletonAsValue = singleton;

    static_cast<Block>(singletonAsValue);

    Block singletonInitializer {singleton};

    print(singletonInitializer);

    Block threeEmpties {Block {}, Block {}, Block {}};

    Block randomStuff {"blue", Block {true, 1020}, 3.04};
    print(randomStuff);

    // More tests, waiting for the test suite

    Block blk { {1, true}, {false, 2} };
    std::cout << blk;
    assert(blk.isBlock());
    assert(blk[1].isBlock());
    assert(blk[2].isBlock());

    Block blk1 = static_cast<Block>(blk[1]);
    Block blk2 = static_cast<Block>(blk[2]);
    assert(blk1[1].isInteger());
    assert(blk1[2].isLogic());
    assert(blk2[1].isLogic());
    assert(blk2[2].isInteger());
}
