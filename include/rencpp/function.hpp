#ifndef RENCPP_FUNCTION_HPP
#define RENCPP_FUNCTION_HPP

//
// function.hpp
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
//=////////////////////////////////////////////////////////////////////////=//
//
// `ren::Function` can be used to refer to any Rebol FUNCTION!, which is
// interesting (such as to accomplish tasks like "running PARSE from C++").
//
// Another goal of this class is to allow the generation of *new* FUNCTION!s,
// which are backed by a C++ implementation.  This is a "modern" C++11 take on
// that process, which uses template metaprogramming to analyze the type
// signature of a lambda function (or, if you prefer, any "callable" object
// with an `operator()`) to be triggered by the interpreter.
//
// Making it possible to call an arbitrary-arity C++ function from the C
// code of Rebol is somewhat tricky.  :-/  When a Rebol FUNCTION! is called,
// the native code that runs is dispatched through an ordinary C protocol,
// where all the dispatchers look like:
//
//     REB_R Some_Dispatcher(struct Reb_Frame *f)
//
// The single input parameter is a Rebol "frame", which contains the state
// of the evaluator at the moment of that function call.  That includes the
// computed arguments from the callsite, the label (if any) of the word via
// which the function was invoked, and the cell address where the dispatcher
// is supposed to write its return result.
//
// The C sources for Rebol itself use a preprocessing phase to generate
// convenience macros that are #include'd into the files that implement
// dispatchers.  These macros make argument extraction into named local
// variables easier.  (See ARG(), REF(), and `INCLUDE_PARAMS_OF_XXX` macros)
//
// But RenCpp experiments with the use of "modern C++" to make templates that
// automatically analyze multiple-arity lambda functions which take
// `ren::AnyValue` subclasses.  Without any separate preprocessing phase,
// it can generate a unique single-arity "shim" for each.  Rebol then calls
// the shim, which unpacks the arguments and proxies the return result.
//
// By contrast, the Red language's "libRed" reaches underneath the level of C
// to implement natural-looking calls variadically:
//
// https://doc.red-lang.org/en/libred.html#_registering_a_callback_function
//
// The effect is similar, although it makes an assumption about the ABI:
//
// https://en.wikipedia.org/wiki/Application_binary_interface
//
// ...and because the technique is tied to plain C function pointers, it can't
// wrap up `std::function` instances the way this template code can.  That
// means you can dynamically generate Rebol FUNCTION!s based on std::function
// objects which have been bound with instance data dynamically.
//
// All that said--this was a largely experimental C++11 codebase to see what
// was technically possible.  It may be that simpler techniques would be
// preferable.  Yet this does work!
//

#include <cassert>
#include <functional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <memory> // for std::unique_ptr
#include <mutex> // global table must be protected for thread safety

#include "value.hpp"
#include "atoms.hpp"
#include "arrays.hpp"
#include "error.hpp"

#include "engine.hpp"

namespace ren {

namespace internal {

//
// RenShimPointer
//
// As far as clients of RenCpp are concerned, struct Reb_Frame is an opaque
// type, and the REB_R values are not published.  Ideally, no dispatcher code
// or the data types they depend on would appear to pollute header files.  Yet
// there is a constraint in C++ that templates must be in the headers:
//
// https://isocpp.org/wiki/faq/templates#templates-defn-vs-decl
//
// As a consequence, if you define a RenCpp function taking ren::Integer and
// another that takes a ren::Block and a ren::String...they will require
// distinct C function "shims" to unpack a single parameter and type check it
// vs. one that takes two parameters and type checks that.  This is the price
// of working with "template magic".
//
// Because this common archetype is used by varying types of C++ function
// objects, the `cppfun` is a void pointer.  But the specific implementation
// of the shim in the template knows which type to cast to when it runs.
//
// !!! Should the engine handle come implicitly from the frame?  It seems
// that a frame needs to know this.  For that matter, ->out can be derived
// from the frame as well, and doesn't need to be a separate parameter.
//
using RenShimPointer = void (*)(
    REBVAL *out,
    RenEngineHandle engine,
    const void *cppfun, // each function type signature has its own shim
    struct Reb_Frame *f // frame gives access to args and other properties
);

//
// How long the C++ function object has to stick around depends on how long
// it takes for the wrapping FUNCTION! to be garbage collected.  In order
// for the generic C code inside Rebol to be able to free the C++ function,
// there has to be a C-style routine to call to do it for that specific
// function subtype.
//
using RenCppfunFreer = void (*)(void *cppfun);

}


//
// FUNCTION TYPE
//

//
// Function is the interface for FUNCTION! values that may-or-may-not have a
// C++ implementation.  Because it's not possible to have a term be
// both a template and a class, the template magic is done inside of
// FunctionGenerator so that one doesn't have to write Function<> when just
// working with ordinary function values.  The FunctionGenerator is then
// hidden behind Function::construct.
//
// !!! Should it be Function::make, or does that interfere with potential
// other meanings for MAKE?
//

class Function : public AnyValue {
protected:
    friend class AnyValue;
    Function (Dont) : AnyValue (Dont::Initialize) {}
    static bool isValid(REBVAL const * cell);

private:
    //
    // This static function can't be a member of FunctionGenerator and moved
    // to the implementation file, because FunctionGenerator is a template.
    // But it's the "real" dispatcher which is used, that then delegates to
    // the specific templated "shim" function to unpack the arguments.
    //
    static int32_t Ren_Cpp_Dispatcher(struct Reb_Frame *f);

    // Most classes can get away with setting up cell bits all in the
    // implementation files, but FunctionGenerator is a template.  It
    // needs to be able to finalize "in view".  We might consider another
    // way of shaping this, by having a public "set cell bits for function"
    // API in the hooks.h, then just use normal finishInit.  Might be what
    // has to be done.

    template <class R, class... Ts>
    friend class internal::FunctionGenerator;

    void finishInitSpecial(
        RenEngineHandle engine,
        Block const & spec,
        internal::RenShimPointer shim,
        void *cppfun, // a std::function that shim knows how to interpret
        internal::RenCppfunFreer freer
    );


    // The FunctionGenerator is an internal class.  One reason why the
    // interface is exposed as a function instead of as a class is because
    // of a problem: C++11 is missing the feature of a convenient way to use
    // type inference from a lambda to determine its signature.  So you can't
    // make a templated class that automatically detects and extracts their
    // type when passed as an argument to the constructor.  This creates an
    // annoying repetition where you wind up typing the signature twice...once
    // on what you're trying to pass the lambda to, once on the lambda itself.
    //
    //     Foo<Baz(Bar)> temp { [](Bar bar) -> Baz {...} }; // booo!
    //
    //     auto temp = makeFoo { [](Bar bar) -> Baz {...} }; // better!
    //
    // Eliminating the repetition is desirable.  So Morwenn adapted this
    // technique from one of his StackOverflow answers:
    //
    //     http://stackoverflow.com/a/19934080/211160
    //
    // It will not work in general cases with "Callables", e.g. objects that
    // have operator() overloading.  (There could be multiple overloads, which
    // one to pick?  For a similar reason it will not work with "Generic
    // Lambdas" from C++14.)  But it works for ordinary lambdas...and that
    // is the common case and hence worth addressing.
    //

public:
    template<typename Fun, std::size_t... Ind>
    static Function construct_(
        std::true_type, // Fun return type is void
        RenEngineHandle engine,
        Block const & spec,
        Fun && cppfun,
        utility::indices<Ind...>
    ) {
        // Handling `return void` serves the purpose of removing the need for
        // a return statement, but it also has to be a way of returning the
        // "correct" value.  In Rebol and Red the default return value is
        // no value, which does not have a concrete type...it's the disengaged
        // state of an `optional<AnyValue>`.

        using Ret = optional<AnyValue>;

        using Gen = internal::FunctionGenerator<
            Ret,
            utility::argument_type<Fun, Ind>...
        >;

        return Gen {
            engine,
            spec,
            std::function<
                Ret(utility::argument_type<Fun, Ind>...)
            >([&cppfun](utility::argument_type<Fun, Ind>&&... args){
                cppfun(std::forward<utility::argument_type<Fun, Ind>>(args)...);
                return nullopt;
            })
        };
    }

    template<typename Fun, std::size_t... Ind>
    static Function construct_(
        std::false_type,    // Fun return type is not void
        RenEngineHandle engine,
        Block const & spec,
        Fun && cppfun,
        utility::indices<Ind...>
    ) {
        using Ret = utility::result_type<Fun>;

        using Gen = internal::FunctionGenerator<
            Ret,
            utility::argument_type<Fun, Ind>...
        >;

        return Gen {
            engine,
            spec,
            std::function<
                Ret(utility::argument_type<Fun, Ind>...)
            >(cppfun)
        };
    }

    template<typename Fun>
    static Function construct_(
        RenEngineHandle engine,
        Block const & spec,
        Fun && cppfun
    ) {
        using Ret = utility::result_type<Fun>;

        using Indices = utility::make_indices<
            utility::function_traits<Fun>::arity
        >;

        return construct_(
            typename std::is_void<Ret>::type{},  // tag dispatching
            engine,
            spec,
            std::forward<Fun>(cppfun),
            Indices{}
        );
    }


    //
    // For convenience, we define specializations that let you be explicit
    // about the engine and/or provide an already built spec block.
    //

    template<typename Fun>
    static Function construct(
        char const * spec,
        Fun && cppfun
    ) {
        return construct_(
            Engine::runFinder().getHandle(),
            Block {spec},
            std::forward<Fun>(cppfun)
        );
    }


    template<typename Fun>
    static Function construct(
        Block const & spec,
        Fun && cppfun
    ) {
        return construct_(
            Engine::runFinder().getHandle(),
            spec,
            std::forward<Fun>(cppfun)
        );
    }


    template<typename Fun>
    static Function construct(
        Engine & engine,
        char const * spec,
        Fun && cppfun
    ) {
        return construct_(
            engine,
            Block {spec},
            std::forward<Fun>(cppfun)
        );
    }


    template<
        typename Fun,
        typename Indices = utility::make_indices<
            utility::function_traits<Fun>::arity
        >
    >
    static Function construct(
        Engine & engine,
        Block const & spec,
        Fun && cppfun
    ) {
        return construct_(
            engine.getHandle(),
            spec,
            std::forward<Fun>(cppfun)
        );
    }

    // This apply convenience overload used to be available to all values,
    // but it really only makes sense for a few value types.
public:
    template <typename... Ts>
    inline optional<AnyValue> operator()(Ts &&... args) const {
        return apply(std::forward<Ts>(args)...);
    }
};


//
// EXTENSION FUNCTION TEMPLATE
//

//
// There may be ways of making the spec block automatically, but naming
// the parameters would be difficult.  A version that took a single
// argument and just called them arg1 arg2 etc and built the type
// strings may be possible in the future, but that's really not a good
// way to document your work even if it's technically achievable.
//

//
// Note that inheriting from std::function would probably be a bad idea:
//
//     http://stackoverflow.com/q/27263092/211160
//


namespace internal {

template<class R, class... Ts>
class FunctionGenerator : public Function {
private:

    //
    // Rebol natives take in a pointer to the frame.  Today, we extract a
    // value pointer for a given frame argument using RL_Arg().
    //

    using FunType = std::function<R(Ts...)>;

    using ParamsType = std::tuple<Ts...>;

    // Function used to create Ts... on the fly and apply a
    // given function to them

    template <std::size_t... Indices>
    static auto applyCppFunImpl(
        RenEngineHandle engine,
        FunType const & cppfun,
        struct Reb_Frame *f,
        utility::indices<Indices...>
    )
        -> decltype(
            cppfun(
                AnyValue::fromCell_<
                    typename std::decay<
                        typename utility::type_at<Indices, Ts...>::type
                    >::type
                >(
                    RL_Arg(f, Indices + 1), // Indices are 0-based
                    engine
                )...
            )
        )
    {
        return cppfun(
            AnyValue::fromCell_<
                typename std::decay<
                    typename utility::type_at<Indices, Ts...>::type
                >::type
            >(
                RL_Arg(f, Indices + 1), // Indices are 0 based
                engine
            )...
        );
    }

    template <typename Indices = utility::make_indices<sizeof...(Ts)>>
    static auto applyCppFun(
        RenEngineHandle engine, FunType const & cppfun, struct Reb_Frame *f
    ) ->
        decltype(applyCppFunImpl(engine, cppfun, f, Indices {}))
    {
        return applyCppFunImpl(engine, cppfun, f, Indices {});
    }

private:
    static void shim(
        REBVAL *out,
        RenEngineHandle engine,
        const void *cppfun,
        struct Reb_Frame * f
    ){
        // Our applyCppFun helper does the magic to recursively forward
        // the AnyValue classes that we generate to the function that
        // interfaces us with the Callable the extension author wrote
        // (who is blissfully unaware of the call frame convention and
        // writing using high-level types...)
        //
        // Note: All the logic for handling exceptions is contained in the
        // Ren_Cpp_Dispatcher(), which wraps this in a `try` (the code here
        // is minimal to reduce the amount of internals that are exposed in
        // the header to just what's necessary to get the template working)

        auto && temp = applyCppFun(
            engine,
            *reinterpret_cast<const FunType*>(cppfun),
            f
        );

        // The return result is written into a location that is known
        // according to the protocol of the call frame

        AnyValue::toCell_(out, temp); // temp may be ren::optional
    }

    // Note this is a template, and so there is a different "freer" function
    // generated for each instance.  That means the `delete` has to be against
    // this `FunType`, and it has to happen in the header file.
    //
    static void freer(void *cppfun) {
        delete reinterpret_cast<FunType*>(cppfun); 
    }

public:
    FunctionGenerator (
        RenEngineHandle engine,
        Block const & spec,
        FunType const & cppfun
    ) :
        Function (Dont::Initialize)
    {
        // We are receiving a std::function that implements the C++ "body" of
        // the Ren function.  But the entity that will be holding onto it for
        // its lifetime is a Ren-C FUNCTION! body...which can only stow it
        // in a void pointer of a handle.
        //
        // Hence, we need to pass in a freeing function as well.

        // We've got what we need, but depending on the runtime it will have
        // a different encoding of the shim and type into the bits of the
        // cell.  We defer to a function provided by each runtime.

        Function::finishInitSpecial(
            engine,
            spec,
            &shim,
            new FunType {cppfun},
            &freer
        );
    }
};

} // end namespace internal

} // end namespace ren

#endif
