#ifndef RENCPP_ENGINE_HPP
#define RENCPP_ENGINE_HPP

//
// exceptions.hpp
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

#include <functional>
#include <iostream>
#include <stdexcept>

#include "value.hpp"
#include "runtime.hpp"

namespace ren {


//
// ENGINE OBJECT FOR SANDBOXING INTERPRETER STATE
//

//
// Each Engine represents a kind of "sandbox", so setting a variable "x"
// in one does not mean it will be readable in another one.  This means that
// operations like getting words and values need to know which one you mean.
//
// You can always be explicit about which environment to use.  But if you are
// not...then a global handler is called to give back a reference to the one
// that is "currently in effect".  The handler you register takes no parameters
// and must rely on some state-known-to-it to decide (such as which thread
// you are on).
//
// For "making simple things simple", there is a default handler.  If you make
// any calls to manipulate Values or call into the runtime before registering
// a different one, that handler will automatically allocate an environment
// for you that will have a lifetime through the end of the program.
//


class Engine {
public:
    using Finder = std::function<Engine&()>;

private:
    friend class Value;
    friend class AnyArray;
    friend class AnyString;
    friend class AnyWord;
    friend class Context;

    RenEngineHandle handle;

    static Finder finder;

    // These should maybe be internalized behind the binding and not data
    // members of the C++ class
private:
    std::ostream * osPtr;
    std::istream * isPtr;

public:
    // Disable copy construction and assignment.

    Engine (Engine const & other) = delete;
    Engine & operator= (Engine const & other) = delete;


public:
    Engine () :
        osPtr (&std::cout),
        isPtr (&std::cin)
    {
        if (::RenAllocEngine(&handle) != 0) {
            throw std::runtime_error ("Couldn't initialize red runtime");
        }
    }

    RenEngineHandle getHandle() const {
        return handle;
    }

    static Finder setFinder(
        Finder const & newFinder
    ) {
        auto result = finder;
        finder = newFinder;
        return result;
    }

    static Engine & runFinder() {
        if (not finder) {
            finder = [] () -> Engine & {
                static Engine global;
                return global;
            };
        }

        return finder();
    }


    //
    // As a C++ library it seemed fitting to let you hook your own istream
    // and ostream up for the "host kit".  Writing a custom iostream is a
    // bit confusing, however, and it may not be the best interface.
    //
    // One could easily imagine a different Engine having a different place
    // it is streaming its output to.
    //
public:
    std::ostream & setOutputStream(std::ostream & os);

    std::istream & setInputStream(std::istream & is);

    std::ostream & getOutputStream();

    std::istream & getInputStream();

    //
    // See notes on how close() is used for catching exceptions, while the
    // destructor should not throw:
    //
    //     http://stackoverflow.com/a/130123/211160
    //

    void close() {
        auto releaseMe = handle;
        handle = REN_ENGINE_HANDLE_INVALID;
        if (::RenFreeEngine(releaseMe) != 0) {
            throw std::runtime_error ("Failed to shut down red environment");
        }
    }


    virtual ~Engine() {
        if (not REN_IS_ENGINE_HANDLE_INVALID(handle))
            ::RenFreeEngine(handle);
    }

public:
    static Value evaluate(
        std::initializer_list<internal::BlockLoadable<Block>> loadables,
        Engine & engine
    );

    template <typename... Ts>
    Value operator()(Ts... args) {
        return evaluate({args...}, *this);
    }
};

} // end namespace ren

#endif
