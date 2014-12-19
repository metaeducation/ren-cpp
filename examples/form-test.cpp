#include <iostream>
#include <string>
#include <cassert>

#include "rencpp/ren.hpp"

using namespace ren;

int main(int, char **) {
    assert(static_cast<std::string>(Value {10}) == "10");
    assert(static_cast<std::string>(Value {1.5}) == "1.5");
    assert(static_cast<std::string>(Value {true}) == "true");

    // The only type willing to implicitly cast to a std::string is
    // ren::String; all others must be explicit with a static_cast

    std::string converted = String {"{Hello World}"};
    assert(converted == "Hello World");

    assert(String {"{Hello World}"} == "Hello World");
}
