#ifndef RENCPP_HELPERS_HPP
#define RENCPP_HELPERS_HPP

//
// helpers.hpp
// This file is part of RenCpp
// Copyright (C) 2015 HostileFork.com
//
// Licensed under the Boost License, Version 1.0 (the "License")
//
//      http://www.boost.org/LICENSE_1_0.txt
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied.  See the License for the specific language governing
// permissions and limitations under the License.
//
// See http://rencpp.hostilefork.com for more information on this project
//

#include "engine.hpp"

namespace ren {

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
// This is JUST AN EXPERIMENT to see how people might use it if it were
// available.  By writing it this way and not calling into the evaluator
// it will not match up with what print does necessarily, even if it were
// a complete reimplementation of the default print behavior (it is not).
//

//
// Moving the IO hook to be a "per-Engine property" created the first real
// question about coming up with something like that, so the Engine::runFinder
// calls in here are pretty rough, but we'll see where that goes.
//


class Printer {
public:
    Printer ()
    {
    }

    template <typename T>
    void writeArgs(bool , T && t) {
        Engine::runFinder().getOutputStream() << std::forward<T>(t);
    }

    template <typename T, typename... Ts>
    void writeArgs(bool spaced, T && t, Ts &&... args) {
        writeArgs(spaced, std::forward<T>(t));
        if (spaced)
            Engine::runFinder().getOutputStream() << " ";
        writeArgs(spaced, std::forward<Ts>(args)...);
    }

    template <typename... Ts>
    void corePrint(bool spaced, bool linefeed, Ts &&... args) {
        writeArgs(spaced, std::forward<Ts>(args)...);
        if (linefeed)
            Engine::runFinder().getOutputStream() << std::endl;
    }

    template <typename... Ts>
    void operator()(Ts &&... args) {
        corePrint(true, true, std::forward<Ts>(args)...);
    }

    template <typename... Ts>
    void only(Ts &&... args) {
        corePrint(false, false, std::forward<Ts>(args)...);
    }

    ~Printer () {
    }
};

extern Printer print;

} // end namespace ren

#endif
