#ifndef RENCPP_EXTENSION_HPP
#define RENCPP_EXTENSION_HPP

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
// The dispatch for Rebol functions is through an ordinary C protocol, where
// all the dispatchers look like:
//
//     REB_R Some_Dispatcher(struct Reb_Frame *f)
//
// Dispatchers can get a pointer to the REBFUN (if they need it) via f->func,
// and extract properties from it.  They can also read the argument cells out
// of the frame, through f->arg[N].
//
// One concept in Ren-Cpp is to build a facade that is an actual type-correct
// C++-style signature for functions (that can also be invoked from Rebol).
// This means that when they are called from C++, they can be checked at
// compile time for the right types and number of parameters.  In fact, the
// idea is to be able to implement this for anything that can be operator()
// overloaded.  (e.g. any "Callable")
//
// Yet there has to be some translation between the dispatcher and the C++
// "callable" object.  The code needed to do that translation will be
// different based on the type signature of the C++ object, and has to be
// generated at compile time.  These "shims" are all templated variations on
// a static method of FunctionGenerator.
//
// Because this common archetype is used by varying types of C++ function
// objects, the `cppfun` is a void pointer.  But the specific implementation
// of the shim in the template knows which type to cast to when it runs.
//
// !!! Should the engine handle come implicitly from the frame?  It seems
// that a frame needs to know this.
//
using RenShimPointer = RenResult (*)(
    RenCell *out,
    RenEngineHandle engine,
    const void *cppfun, // each function type signature has its own shim
    RenCell args[] // args passed as contiguous array of cells
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

class Function : public AnyValue {
protected:
    friend class AnyValue;
    Function (Dont) : AnyValue (Dont::Initialize) {}
    static bool isValid(RenCell const * cell);

#ifdef REN_RUNTIME
private:
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


    //
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
    // It will not work in the general case with "Callables", e.g. objects that
    // have operator() overloading.  (There could be multiple overloads, which
    // one to pick?  For a similar reason it will not work with "Generic
    // Lambdas" from C++14.)  However, it works for ordinary lambdas...and that
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
#endif
};


#ifdef REN_RUNTIME

//
// EXTENSION FUNCTION TEMPLATE
//

//
// While calling Ren and the runtime from C++ is interesting (such as to
// accomplish tasks like "running PARSE from C++"), a potentially even
// more relevant task is to make it simple to call C++ code from inside
// the Rebol or Red system.  Goals for such an interface would be type-safety,
// brevity, efficiency, etc.
//
// This is a "modern" C++11 take on that process.  It uses template
// metaprogramming to analyze the type signature of a lambda function (or,
// if you prefer, anything else with an operator()) to be called for the
// implementation, and unpacks the parameters to call it with.  See the
// tests for the notation, but it is rather pretty.
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
    // Rebol natives take in a pointer to the stack of REBVALs.  This stack
    // has protocol for the offsets of arguments, offsets for other
    // information (like where the value for the function being called is
    // written), and an offset where to write the return value:
    //

    using FunType = std::function<R(Ts...)>;

    using ParamsType = std::tuple<Ts...>;

    // Function used to create Ts... on the fly and apply a
    // given function to them

    template <std::size_t... Indices>
    static auto applyCppFunImpl(
        RenEngineHandle engine,
        FunType const & cppfun,
        RenCell *args,
        utility::indices<Indices...>
    )
        -> decltype(
            cppfun(
                AnyValue::fromCell_<
                    typename std::decay<
                        typename utility::type_at<Indices, Ts...>::type
                    >::type
                >(
                    &args[Indices],
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
                &args[Indices],
                engine
            )...
        );
    }

    template <typename Indices = utility::make_indices<sizeof...(Ts)>>
    static auto applyCppFun(
        RenEngineHandle engine, FunType const & cppfun, RenCell *args
    ) ->
        decltype(applyCppFunImpl(engine, cppfun, args, Indices {}))
    {
        return applyCppFunImpl(engine, cppfun, args, Indices {});
    }

private:
    static RenResult shim(
        RenCell *out,
        RenEngineHandle engine,
        const void *cppfun,
        RenCell args[]
    ){
        // To be idiomatic for C++, we want to be able to throw a ren::Error
        // using C++ exceptions from within a ren::Function.  Yet since the
        // ren runtimes cannot catch C++ exceptions, we have to translate it
        // here...as well as make decisions about any non-ren::Error
        // exceptions the C++ code has thrown.

        // Only initialized if `success and exiting`, and we don't really need
        // to initialize it and would rather not; but Clang complains as it
        // cannot prove it's always initialized when used.  Revisit.

        RenResult result;

        try {
            // Our applyCppFun helper does the magic to recursively forward
            // the AnyValue classes that we generate to the function that
            // interfaces us with the Callable the extension author wrote
            // (who is blissfully unaware of the call frame convention and
            // writing using high-level types...)

            auto && temp = applyCppFun(
                engine,
                *reinterpret_cast<const FunType*>(cppfun),
                args
            );

            // The return result is written into a location that is known
            // according to the protocol of the call frame

            AnyValue::toCell_(out, temp); // temp may be ren::optional
            result = REN_SUCCESS;
        }
        catch (bad_optional_access const &) {
            throw std::runtime_error {
                "C++ code dereferenced empty optional, no further details"
                " (try setting a breakpoint in optional.hpp)"
            };
        }
        catch (Error const & e) {
            *out = e;
            result = REN_APPLY_ERROR;
        }
        catch (optional<Error> const & e) {
            if (e == nullopt)
                throw std::runtime_error {
                    "ren::nullopt optional<Error> thrown from ren::Function"
                };
            *out = (*e);
            result = REN_APPLY_ERROR;
        }
        catch (AnyValue const & v) {
            // In C++ `throw` is an error mechanism, and using it for general
            // non-localized control (as Rebol uses THROW) is considered abuse
            if (not hasType<Error>(v))
                throw std::runtime_error {
                    "Non-isError() Value thrown from ren::Function"
                };

            *out = v;
            result = REN_APPLY_ERROR;
        }
        catch (optional<AnyValue> const & v) {
            if (not hasType<Error>(v))
                throw std::runtime_error {
                    "Non-isError() optional<AnyValue> thrown from ren::Function"
                };

            *out = *(v->cell);
            result = REN_APPLY_ERROR;
        }
        catch (evaluation_error const & e) {
            // Ren runtime code throws non-std::exception errors, and so
            // we tolerate C++ code doing the same if it's running as if it
            // were the runtime.  But we also allow the evaluation_error that
            // bubbles up through C++ to be extracted as an error to thread
            // back into the Ren runtime.  This way you can write a C++
            // routine that doesn't know if it's being called by other C++
            // code (hence having std::exception expectations) or just as
            // the implementation of a ren::Function

            *out = (e.error());
            result = REN_APPLY_ERROR;
        }
        catch (evaluation_throw const & t) {
            // We have to fabricate a THROWN() name label with the actual
            // thrown value stored aside.  Making pointer reference
            // temporaries is needed to suppress a compiler warning:
            //
            //    http://stackoverflow.com/a/2281928/211160

            const RenCell * thrown_value =
                t.value() == nullopt
                    ? nullptr
                    : t.value()->cell;

            const RenCell * thrown_name =
                t.name() == nullopt
                    ? nullptr
                    : t.name()->cell;

            RenShimInitThrown(out, thrown_value, thrown_name);
            result = REN_APPLY_THREW;
        }
        catch (load_error const & e) {
            *out = e.error();
            result = REN_CONSTRUCT_ERROR;
        }
        catch (evaluation_halt const &) {
            result = REN_EVALUATION_HALTED;
        }
        catch (std::exception const & e) {

            std::string what = e.what();

            // Theoretically we could catch the C++ exception, poke its
            // string into a cell, and tell the ren runtime that's what
            // happened.  For now we just rethrow

            throw;
        }
        catch (...) {

            // Mystery error.  No string available.  Again, we could say
            // "mystery C++ object thrown" and write it into an error we
            // pass back to the ren runtime, but for now we rethrow

            throw std::runtime_error {
                "Exception from ren::Function not std::Exception or Error"
            };
        }

        // We now should have all the C++ objects cleared from this stack,
        // so at least as far as THIS function is concerned, a longjmp
        // should be safe (which is what Rebol will do if we call RenShimError
        // or RenShimExit).

        switch (result) {
        case REN_SUCCESS:
            // !!! R_OUT is 5 in Rebol and so is REN_SUCCESS
            return REN_SUCCESS;

        case REN_APPLY_THREW:
            // !!! R_OUT_THREW is 1 in Rebol and so is REN_APPLY_THREW
            return REN_APPLY_THREW;

        case REN_EVALUATION_HALTED:
            return RenShimHalt();

        case REN_APPLY_ERROR:
        case REN_CONSTRUCT_ERROR:
            return RenShimFail(out);

        default:
            UNREACHABLE_CODE();
        }
    }

    // Note this is a template, and so there is a different "freer" function generated for each
    // instance.  That means the `delete` has to be against this `FunType`, and it has to
    // happen in the header file.
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


#endif

}

#endif
