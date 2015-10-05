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
// interchangeably, although the data type was called OBJECT!  RenCpp embraces
// the terminology notion that really "object" isn't a very good name for what
// it does; and standard expectations don't apply.  So there is no ren::Object,
// only ren::Context.
//
// Under Rebol's hood, an object was implemented as a series, but lacking any
// of the positional properties.
//

class Engine;

class Context : public AnyValue {
protected:
    friend class AnyValue;
    Context (Dont) noexcept : AnyValue (Dont::Initialize) {}
    inline bool isValid() const { return isContext(); }
public:
    Context copy(bool deep = true) {
        return static_cast<Context>(AnyValue::copy(deep));
    }

public:
    using Finder = std::function<Context (Engine *)>;

private:
    friend class AnyArray;
    friend class AnyString;
    friend class AnyWord;
    friend class Runtime;

    static Finder finder;
    RenEngineHandle getEngine() const { return origin; }

public:

    static Context lookup(char const * name, Engine * engine = nullptr);

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

    static Context current(Engine * engine = nullptr);


    // Patterned after AnyArray; you can construct a context from the same
    // data that can be used to make a block.
protected:
    Context (
        internal::Loadable const loadables[],
        size_t numLoadables,
        Context const * contextPtr,
        Engine * engine
    );

    Context (
        AnyValue const values[],
        size_t numValues,
        Context const * contextPtr,
        Engine * engine
    );


public:
    Context (
        AnyValue const values[],
        size_t numValues,
        Context const & context
    ) :
        Context (values, numValues, &context, nullptr)
    {
    }

    Context (
        AnyValue const values[],
        size_t numValues,
        Engine * engine
    ) :
        Context (values, numValues, nullptr, engine)
    {
    }

    Context (
        std::initializer_list<internal::Loadable> const & loadables,
        Context const & context // is this contradictory?
    ) :
        Context (loadables.begin(), loadables.size(), &context, nullptr)
    {
    }

    Context (
        std::initializer_list<internal::Loadable> const & loadables,
        Engine * engine = nullptr
    ) :
        Context (loadables.begin(), loadables.size(), nullptr, engine)
    {
    }

    Context (Engine * engine = nullptr) :
        Context (static_cast<internal::Loadable *>(nullptr), 0, nullptr, engine)
    {
    }

    // If you use the apply operation in a context, then it means "do this
    // code in this context"
public:
    template <typename... Ts>
    inline optional<AnyValue> operator()(Ts &&... args) const {
        return apply({std::forward<Ts>(args)...}, internal::ContextWrapper {*this});
    }

    template <typename R, typename... Ts>
    inline R create(Ts &&... args) const {
        return R {{std::forward<Ts>(args)...}, internal::ContextWrapper {*this}};
    }
};

} // end namespace ren

#endif
