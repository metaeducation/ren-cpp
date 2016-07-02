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

#include "value.hpp"
#include "runtime.hpp"

namespace ren {


//
// CONTEXT FOR BINDING
//

//
// Historically the Rebol language used the terms CONTEXT and OBJECT somewhat
// interchangeably, although the data type was called OBJECT!  Ren-C has
// redefined the terminology so that ANY-CONTEXT! is the superclass of
// ERROR!, OBJECT!, PORT! etc. (in the spirit of not having the superclass
// share a name with any specific member of said class).
//
// !!! Under Rebol's hood, an object was implemented as a pair of series.
// Accessing the object by position was not allowed, though some natives
// offered features that demonstrated positional awareness (e.g. SET).
// This is likely to be deprecated in favor of more fluidity in the
// implementation of object.
//

class Engine;

class AnyContext : public AnyValue {
protected:
    friend class AnyValue;
    AnyContext (Dont) noexcept : AnyValue (Dont::Initialize) {}
    static bool isValid(RenCell const * cell);

    // Friending doesn't seem to be enough for gcc 4.6, see SO writeup:
    //    http://stackoverflow.com/questions/32983193/
public:
    friend class Object;
    static void initObject(RenCell * cell);
    friend class Error;
    static void initError(RenCell * cell);
    //
    // !!! Ports, Modules, Frames... (just Object and error for starters)
    //

public:
    AnyContext copy(bool deep = true) {
        return static_cast<AnyContext>(AnyValue::copy(deep));
    }

public:
    using Finder = std::function<AnyContext (Engine *)>;

private:
    friend class AnyArray;
    friend class AnyString;
    friend class AnyWord;
    friend class Runtime;

    static Finder finder;
    RenEngineHandle getEngine() const { return origin; }

public:

    static AnyContext lookup(char const * name, Engine * engine = nullptr);

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

    static AnyContext current(Engine * engine = nullptr);


    // Patterned after AnyArray; you can construct a context from the same
    // data that can be used to make a block.
protected:
    AnyContext (
        internal::Loadable const loadables[],
        size_t numLoadables,
        internal::CellFunction F,
        AnyContext const * contextPtr,
        Engine * engine
    );

    AnyContext (
        AnyValue const values[],
        size_t numValues,
        internal::CellFunction F,
        AnyContext const * contextPtr,
        Engine * engine
    );


    // If you use the apply operation in a context, then it means "do this
    // code in this context"
    //
    // !!! Should this be under #ifdef REN_RUNTIME or trust it to be taken
    // care of otherwise?  Might a better error message be delivered if
    // there is a static assert here?
    //
public:
    template <typename... Ts>
    inline optional<AnyValue> operator()(Ts &&... args) const {
        return apply(
            {std::forward<Ts>(args)...},
            internal::ContextWrapper {*this}
        );
    }

    template <typename R, typename... Ts>
    inline R create(Ts &&... args) const {
        return R {
            {std::forward<Ts>(args)...},
            internal::ContextWrapper {*this}
        };
    }
};


namespace internal {

//
// ANYCONTEXT_ SUBTYPE HELPER
//

template <class C, CellFunction F>
class AnyContext_ : public AnyContext {
protected:
    friend class AnyValue;
    AnyContext_ (Dont) : AnyContext (Dont::Initialize) {}

public:
    AnyContext_ (
        AnyValue const values[],
        size_t numValues,
        internal::ContextWrapper const & wrapper
    ) :
        AnyContext (values, numValues, F, &wrapper.context, nullptr)
    {
    }

    AnyContext_ (
        AnyValue const values[],
        size_t numValues,
        Engine * engine
    ) :
        AnyContext (values, numValues, F, nullptr, engine)
    {
    }

    AnyContext_ (
        std::initializer_list<Loadable> const & loadables,
        internal::ContextWrapper const & wrapper
    ) :
        AnyContext (
            loadables.begin(),
            loadables.size(),
            F,
            &wrapper.context,
            nullptr
        )
    {
    }

    AnyContext_ (AnyContext const & context) :
        AnyContext (static_cast<Loadable *>(nullptr), 0, F, &context, nullptr)
    {
    }

    AnyContext_ (
        std::initializer_list<Loadable> const & loadables,
        Engine * engine = nullptr
    ) :
        AnyContext (loadables.begin(), loadables.size(), F, nullptr, engine)
    {
    }

    AnyContext_ (Engine * engine = nullptr) :
        AnyContext (static_cast<Loadable *>(nullptr), 0, F, nullptr, engine)
    {
    }
};

} // end namespace internal



//
// CONCRETE CONTEXT TYPES
//

//
// For why these are classes and not typedefs:
//
//     https://github.com/hostilefork/rencpp/issues/49
//


class Object
    : public internal::AnyContext_<Object, &AnyContext::initObject>
{
    using AnyContext::initObject;

protected:
    static bool isValid(RenCell const * cell);

public:
    friend class AnyValue;
    using internal::AnyContext_<Object, &AnyContext::initObject>::AnyContext_;
};


} // end namespace ren

#endif
