#ifndef RENCPP_CONTEXT_HPP
#define RENCPP_CONTEXT_HPP

//
// context.hpp
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

#include "values.hpp"
#include "runtime.hpp"

namespace ren {


///
/// CONTEXT OBJECT FOR BINDING
///

//
// A Context represents a context for binding.  Words within contexts, once
// bound, are not isolated from each other--for that you need a separate
// Engine object.
//

class Engine;

class Context {
public:
    using Finder = std::function<Context & (Engine *)>;

private:
    friend class Value;
    friend class AnyBlock;
    friend class AnyString;
    friend class AnyWord;

    Engine * enginePtr;
    RenContextHandle handle;
    bool needsFree;

    static Finder finder;

public:
    // Disable copy construction and assignment.

    Context (Context const & other) = delete;
    Context & operator= (Context const & other) = delete;


public:
    Context (Engine & engine);

    Context (Engine & engine, char const * name);

    Context ();

    Context (char const * name);

    RenContextHandle getHandle() const {
        return handle;
    }

    static Finder setFinder(
        Finder const & newFinder
    ) {
        auto result = finder;
        finder = newFinder;
        return result;
    }

    // The reason that context finding is dependent on the engine has to do
    // with the default execution for Engine e; then e(...)
    // If there was only a context finder that didn't depend on the engine,
    // such calls could return a context from the wrong engine.
    //
    // Passing as a pointer in order to be able to optimize out the cases
    // where you don't care, but the parameter is there if you want to use it

    static Context & runFinder(Engine * engine);

    Engine & getEngine();

    // Needs to be a static method, as some people who are calling it only
    // have a handle and no Context object at that point
protected:
    friend class Engine;

public:
    //
    // See notes on how close() is used for catching exceptions, while the
    // destructor should not throw:
    //
    //     http://stackoverflow.com/a/130123/211160
    //

    void close();

    virtual ~Context();

public:
    template <typename... Ts>
    Value operator()(Ts... args) {
        return Runtime::evaluate({args...}, this);
    }
};

} // end namespace ren

#endif
