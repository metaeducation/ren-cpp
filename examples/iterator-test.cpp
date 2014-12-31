#include <iostream>
#include <cassert>

#include "rencpp/ren.hpp"

using namespace ren;

int main(int, char **) {
    Block blk {"1 2 3"};

    auto it = blk.begin();
    assert(it->isEqualTo(1));
    assert(it == blk.begin());
    assert(it != blk.end());

    it++;
    assert((*it).isEqualTo(2));
    assert(it != blk.begin());
    assert(it != blk.end());

    it++;
    assert(it->isEqualTo(3));
    assert(it != blk.begin());
    assert(it != blk.end());

    it++;
    assert(it != blk.begin());
    assert(it == blk.end());
}
