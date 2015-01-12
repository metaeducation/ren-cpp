#include <iostream>
#include <string>
#include <cassert>

#include "rencpp/ren.hpp"

using namespace ren;

int main(int, char **) {
    assert(to_string(Value {10}) == "10");
    assert(to_string(Value {1.5}) == "1.5");
    assert(to_string(Value {true}) == "true");

    // The only type willing to implicitly cast to a std::string is
    // ren::String; all others must be explicit with to_string which
    // performs equivalently to TO-STRING (new behavior, formerly FORM)

    std::string converted = String {"Hello World"};
    assert(converted == "Hello World");

    assert(String {"Hello World"}.isEqualTo("Hello World"));

    // Yes, not a test suite, but one test is better than zero.
    assert(String {"\n\t\u0444"}.isEqualTo("\n\t\u0444"));
    assert(String {"^/^-^(0444)"}.isEqualTo("\n\t\u0444"));
}
