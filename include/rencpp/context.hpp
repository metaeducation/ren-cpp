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
// can't include Engine -- would be circular

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
    static void constructOrApplyInitializeCore(
        RenEngineHandle engineHandle,
        RenContextHandle contextHandle,
        Value const * applicandPtr,
        internal::Loadable * argsPtr,
        size_t numArgs,
        Value * constructResultUninitialized,
        Value * applyOutUninitialized
    );


    void constructOrApplyInitialize(
        Value const * applicandPtr,
        internal::Loadable * argsPtr,
        size_t numArgs,
        Value * constructResultUninitialized,
        Value * applyOutUninitialized
    );

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
        auto loadables = std::array<internal::Loadable, sizeof...(args)>{
            {args...}
        };

        Value result (Value::Dont::Initialize);

        constructOrApplyInitialize(
            nullptr, // no value to apply to; treat "as if" block
            &loadables[0],
            sizeof...(args),
            nullptr, // don't construct it
            &result // apply it
        );

        return result; // move optimized
    }


    //
    // All ren::Value classes support explicit allocation in a specific
    // context via a version of the constructor where the context
    // is given as the first parameter:
    //
    //    ren::SetWord {ctxOne, "whatever"};
    //
    // The problem is that it becomes a little unclear for instance in blocks
    //
    //    ren::Block {ctxOne, varOne, varTwo, varThree};
    //
    // The compiler detects that ctxOne is a Context and does the right
    // thing (and in fact is used here under the hood).  Yet to a casual
    // reader it looks like envOne is part of the block.
    //
    // This alternate notation is exactly the same and less confusing:
    //
    //     ctxOne<ren::Block>.construct(varOne, varTwo, varThree);
    //

    template <class V, typename... Ts>
    V construct(Ts... args) {
        return V (*this, args...);
    }
};

} // end namespace ren

#endif
