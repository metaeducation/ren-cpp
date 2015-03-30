#ifndef RENCPP_BLOCKS_HPP
#define RENCPP_BLOCKS_HPP

//
// blocks.hpp
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

#include "value.hpp"
#include "series.hpp"

namespace ren {

class AnyBlock : public Series {
protected:
    friend class Value;
    AnyBlock (Dont) noexcept : Series (Dont::Initialize) {}
    inline bool isValid() const { return isAnyBlock(); }

protected:
    //
    // Provide a helper to the derived classes to construct AnyBlock
    // instances through the binding.  But you can't construct an "AnyBlock"
    // instance without a block type... it's abstract!  No such thing.
    //
    // For the curious: empty constructor has to result in an empty block.
    // No way to distinguish from the default constructor:
    //
    //     http://stackoverflow.com/a/9020606/211160
    //

    AnyBlock (
        internal::Loadable const loadables[],
        size_t numLoadables,
        internal::CellFunction cellfun,
        Context const * contextPtr,
        Engine * engine
    );

    AnyBlock (
        Value const values[],
        size_t numValues,
        internal::CellFunction cellfun,
        Context const * contextPtr,
        Engine * engine
    );
};



namespace internal {

//
// ANYBLOCK_ SUBTYPE HELPER
//

//
// BracesT corresponds to the kind of type which is constructed
// when curly braces are encountered in the initialization of a
// block type. Let's consider this example:
//
// Foo foo { 1, { true, false }, {} };
//
// If BracesT is void, you will get a compile time error. But if
// BracesT is Foo, the example will be equivalent to:
//
// Foo foo { 1, Foo { true, false }, Foo {} };
//
// On the other hand, if BracesT is Block, then the example will
// be equivalent to:
//
// Foo foo { 1, Block { true, false }, Block {} };
//
// When untyped braces are encountered, they follow the rules of
// the closest block type. Let's consider another example:
//
// Foo foo { {}, Block { {}, {} }, {} };
//           #1          #2  #3    #4
//
// In this example, #1 and #4 will follow the bracing rules of
// Foo while #2 and #3 will follow the bracing rules of Block.
//

template <class C, CellFunction F, typename BracesT=void>
class AnyBlock_ : public AnyBlock {
protected:
    friend class Value;
    AnyBlock_ (Dont) : AnyBlock (Dont::Initialize) {}
    inline bool isValid() const { return (this->*F)(nullptr); }

public:
    AnyBlock_ (
        Value const values[],
        size_t numValues,
        internal::ContextWrapper const & wrapper
    ) :
        AnyBlock (values, numValues, F, &wrapper.context, nullptr)
    {
    }

    AnyBlock_ (
        Value const values[],
        size_t numValues,
        Engine * engine
    ) :
        AnyBlock (values, numValues, F, nullptr, engine)
    {
    }

    AnyBlock_ (
        std::initializer_list<BlockLoadable<BracesT>> const & loadables,
        internal::ContextWrapper const & wrapper
    ) :
        AnyBlock (
            loadables.begin(),
            loadables.size(),
            F,
            &wrapper.context,
            nullptr
        )
    {
    }

    AnyBlock_ (Context const & context) :
        AnyBlock (static_cast<Loadable *>(nullptr), 0, F, &context, nullptr)
    {
    }

    AnyBlock_ (
        std::initializer_list<BlockLoadable<BracesT>> const & loadables,
        Engine * engine = nullptr
    ) :
        AnyBlock (loadables.begin(), loadables.size(), F, nullptr, engine)
    {
    }

    AnyBlock_ (Engine * engine = nullptr) :
        AnyBlock (static_cast<Loadable *>(nullptr), 0, F, nullptr, engine)
    {
    }

    // A block can be invoked something like a function via DO, so it makes
    // sense for it to have a way of applying it...but it doesn't take
    // any "parameters"
#ifdef REN_RUNTIME
public:
    inline Value operator()() const {
        return apply();
    }
#endif
};

} // end namespace internal



//
// CONCRETE BLOCK TYPES
//

//
// For why these are classes and not typedefs:
//
//     https://github.com/hostilefork/rencpp/issues/49
//


class Block : public internal::AnyBlock_<Block, &Value::isBlock, Block>
{
public:
    friend class Value;
    using internal::AnyBlock_<Block, &Value::isBlock, Block>::AnyBlock_;
};


class Paren : public internal::AnyBlock_<Paren, &Value::isParen>
{
public:
    friend class Value;
    using internal::AnyBlock_<Paren, &Value::isParen>::AnyBlock_;
};


class Path : public internal::AnyBlock_<Path, &Value::isPath>
{
public:
    friend class Value;
    using internal::AnyBlock_<Path, &Value::isPath>::AnyBlock_;

public:
    template <typename... Ts>
    inline Value operator()(Ts &&... args) const {
        return apply(std::forward<Ts>(args)...);
    }
};


class SetPath : public internal::AnyBlock_<SetPath, &Value::isSetPath>
{
public:
    friend class Value;
    using internal::AnyBlock_<SetPath, &Value::isSetPath>::AnyBlock_;

public:
    template <typename... Ts>
    inline Value operator()(Ts &&... args) const {
        return apply(std::forward<Ts>(args)...);
    }
};


class GetPath : public internal::AnyBlock_<GetPath, &Value::isGetPath>
{
public:
    friend class Value;
    using internal::AnyBlock_<GetPath, &Value::isGetPath>::AnyBlock_;

    // As with GetWord, it can be convenient to think of "using" a GetPath
    // as calling a function with no arguments.
#ifdef REN_RUNTIME
public:
    template <typename... Ts>
    inline Value operator()() const {
        return apply();
    }
#endif
};


class LitPath : public internal::AnyBlock_<LitPath, &Value::isLitPath>
{
public:
    friend class Value;
    using internal::AnyBlock_<LitPath, &Value::isLitPath>::AnyBlock_;
};


} // end namespace ren

#endif
