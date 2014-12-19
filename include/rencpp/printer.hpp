#ifndef RENCPP_PRINTER_HPP
#define RENCPP_PRINTER_HPP


#include <stdexcept>
#include <iostream>
#include <iomanip>


#include "common.hpp"

///
/// PRINTING HELPER CLASS EXPERIMENT
///

//
// One misses Rebol/Red PRINT when doing debugging, so this little printer
// class brings you that functionality via variadic functions.  It's easy
// to use, just say:
//
//     ren::print("This", "will", "have", "spaces");
//
// To not get the spaces, use the .only function call.
//
//     ren::print.only("This", "won't", "be", "spaced");
//
// Since this is only an experiment, and being used to try and make 
// debugging more natural and faster... it flushes lines with std::endl.
// The risk of the experiment is that it might wind up being a
// reimplementation on the C++ side and not do *exactly* what the internal
// print routine would do.
//

namespace ren {


class Printer {
    std::ostream & dest;

public:
    Printer (std::ostream & dest) :
        dest (dest)
    {
    }

    template <typename T>
    void writeArgs(bool spaced, T t) {
        UNUSED(spaced);
        dest << t;
    }

    template <typename T, typename... Ts>
    void writeArgs(bool spaced, T t, Ts... args) {
        writeArgs(spaced, t);
        if (spaced)
            dest << " ";
        writeArgs(spaced, args...);
    }

    template <typename... Ts>
    void corePrint(bool spaced, bool linefeed, Ts... args) {
        writeArgs(spaced, args...);
        if (linefeed)
            dest << std::endl;
    }

    template <typename... Ts>
    void operator()(Ts... args) {
        corePrint(true, true, args...);
    }

    template <typename... Ts>
    void only(Ts... args) {
        corePrint(false, false, args...);
    }

    ~Printer () {
    }
};

} // end namespace ren


#endif
