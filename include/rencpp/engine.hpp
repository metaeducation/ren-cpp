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

#include "values.hpp"
#include "context.hpp" // needed for template execution of operator()


namespace ren {


///
/// ENGINE OBJECT FOR SANDBOXING INTERPRETER STATE
///

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
    friend class AnyBlock;
    friend class AnyString;
    friend class AnyWord;
    friend class Context;

    RenEngineHandle handle;

    static Finder finder;


public:
    // Disable copy construction and assignment.

    Engine (Engine const & other) = delete;
    Engine & operator= (Engine const & other) = delete;


public:
    Engine () {
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
    template <typename... Ts>
    Value operator()(Ts... args) {
        auto loadables = std::array<internal::Loadable, sizeof...(args)>{
            {args...}
        };

        Value result (Value::Dont::Initialize);

        Context::runFinder(this).constructOrApplyInitialize(
            nullptr, // no value to apply to; treat "as if" block
            &loadables[0],
            sizeof...(args),
            nullptr, // don't construct it
            &result // apply it
        );

        return result; // move optimized
    }
};

} // end namespace ren

#endif
