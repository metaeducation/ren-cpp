// We only do this if we've built for Rebol

#include "rencpp/rebol.hpp"

using namespace rebol;

int main (int, char**) {
    runtime.doMagicOnlyRebolCanDo();
}
