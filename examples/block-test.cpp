#include <iostream>

#include "rencpp/ren.hpp"

// No reason to abbreviate this, just demonstrating you can
using BLK = ren::Block;

int main(int, char **) {

    BLK empty {};

    BLK threeEmpties {BLK {}, BLK {}, BLK {}};

    BLK randomStuff {"blue", BLK {true, 1020}, 3.04};
}
