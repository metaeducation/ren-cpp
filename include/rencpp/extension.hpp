#ifndef RENCPP_EXTENSION_HPP
#define RENCPP_EXTENSION_HPP

#include <functional>
#include <utility>
#include <unordered_map>
#include <cassert>

#include <mutex> // global table must be protected for thread safety

#include "values.hpp"
#include "engine.hpp"
#include "runtime.hpp"

namespace ren {

///
/// EXTENSION FUNCTION TEMPLATE
///

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

//
// Limits of the type system and specialization force us to have a table
// of functions on a per-template specialization basis.  However, there's
// no good reason to have one mutex per table.  One for all will do.
//

namespace internal {
    extern std::mutex extensionTablesMutex;
}

template<class R, class... Ts>
class Extension : public Function {
private:

    //
    // Rebol natives take in a pointer to the stack of REBVALs.  This stack
    // has protocol for the offsets of arguments, offsets for other
    // information (like where the value for the function being called is
    // written), and an offset where to write the return value:
    //
    //     int  (* REBFUN)(REBVAL * ds);
    //
    // That's too low level for a C++ library.  We wish to allow richer
    // function signatures that can be authored with lambdas (or objects with
    // operator() overloaded, any "Callable").  That means this C-like
    // interface hook ("shim") needs to be generated automatically from
    // examining the type signature, so that it can call the C++ hook.
    //

    typedef std::function<R(Ts...)> FunType;

    typedef std::tuple<Ts...> ParamsType;


    //
    // Although templates can be parameterized with function pointers (or
    // pointer-to-member) there is no way for us to get a std::function
    // object passed as a parameter to the constructor into the C-style shim.
    // The shim, produced by a lambda, must not capture anything from its
    // environment...only accessing global data.  We make a little global
    // map and then capture the data into a static.
    //

    struct TableEntry {
        Engine & engine;
        FunType const fun;
    };

    static std::unordered_map<
        RenShimPointer, TableEntry
    > table;

    static void tableAdd(RenShimPointer shim, TableEntry && entry) {
        using internal::extensionTablesMutex;

        std::lock_guard<std::mutex> lock {extensionTablesMutex};

        table.insert(std::make_pair(shim, entry));
    }

    static TableEntry tableFindAndRemove(RenCell * stack) {
        using internal::extensionTablesMutex;

        std::lock_guard<std::mutex> lock {extensionTablesMutex};

        auto it = table.find(REN_STACK_SHIM(stack));

        TableEntry result = (*it).second;
        table.erase(it);
        return result;
    }

    // Function used to create Ts... on the fly and apply a
    // given function to them

    template <std::size_t... Indices>
    static auto applyFunImpl(
        FunType const & fun,
        Engine & engine,
        RenCell * stack,
        utility::indices<Indices...>
    )
        -> decltype(
            std::forward<FunType const &>(fun)(
                typename utility::type_at<Indices, Ts...>::type{
                    engine,
                    *REN_STACK_ARGUMENT(stack, Indices)
                }...
            )
        )
    {
        return std::forward<FunType const &>(fun)(
            typename utility::type_at<Indices, Ts...>::type{
                engine,
                *REN_STACK_ARGUMENT(stack, Indices)
            }...
        );
    }

    template <
        typename Indices = utility::make_indices<sizeof...(Ts)>
    >
    static auto applyFun(FunType const & fun, Engine & engine, RenCell * stack)
        -> decltype(
            applyFunImpl(
                std::forward<FunType const &>(fun),
                engine,
                stack,
                Indices {}
            )
        )
    {
        return applyFunImpl(
            std::forward<FunType const &>(fun),
            engine,
            stack,
            Indices {}
        );
    }

public:
    Extension (Engine & engine, Block const & spec, FunType const & fun) :
        Function (Dont::Initialize)
    {
        RenShimPointer const & shim  =
            [] (RenCell * stack) -> int {
                // We only need to do this lookup once.  Though the static
                // variable initialization is thread-safe, the table access
                // itself is not...so it is protected via mutex.

                static TableEntry entry = tableFindAndRemove(stack);

                // Our applyFun helper does the magic to recursively forward
                // the Value classes that we generate to the function that
                // interfaces us with the Callable the extension author wrote
                // (who is blissfully unaware of the stack convention and
                // writing using high-level types...)

                auto && result = applyFun(entry.fun, entry.engine, stack);

                // The return result is written into a location that is known
                // according to the protocol of the stack

                *REN_STACK_RETURN(stack) = result.cell;

                // Note: trickery!  R_RET is 0, but all other R_ values are
                // meaningless to Red.  So we only use that one here.

                return REN_SUCCESS;
            };

        // Insert the shim into the mapping table so it can find itself while
        // the shim code is running.  Note that the tableAdd code has
        // to be thread-safe in case two threads try to modify the global
        // table at the same time...

        tableAdd(shim, TableEntry {engine, fun});

        // We've got what we need, but depending on the runtime it will have
        // a different encoding of the shim and type into the bits of the
        // cell.  We defer to a function provided by each runtime.

        Function::finishInit(engine, spec, shim);
    }

    Extension (Block const & spec, FunType const & fun) :
        Extension (Engine::runFinder(), spec, fun)
    {
    }

    Extension (Engine & engine, char const * specCstr, FunType const & fun) :
        Extension (engine, Block {specCstr}, fun)
    {
    }

    Extension (char const * specCstr, FunType const & fun) :
        Extension (Engine::runFinder(), specCstr, fun)
    {
    }
};


//
// There is some kind of voodoo that makes this work, even though it's in a
// header file.  So each specialization of the Extension type gets its own
// copy and there are no duplicate symbols arising from multiple includes
//

template<class R, class... Ts>
std::unordered_map<
    RenShimPointer,
    typename Extension<R, Ts...>::TableEntry
> Extension<R, Ts...>::table;



///
/// UGLY LAMBDA INFERENCE NIGHTMARE
///

//
// This craziness is due to a seeming-missing feature in C++11, which is a
// convenient way to use type inference from a lambda.  Lambdas are the most
// notationally convenient way of making functions, and yet you can't make
// a template which picks up their signature.  This creates an annoying
// repetition where you wind up typing the signature twice.
//
// We are trusting here that it's not a "you shouldn't do it" but rather a
// "the standard library didn't cover it".  This helpful adaptation from
// Morwenn on StackOverflow does the trick.
//
//     http://stackoverflow.com/a/19934080/211160
//
// But putting it in use is as ugly as any standard library implementation
// code, though.  :-P  If you're dealing with "Generic Lambdas" from C++14
// it breaks, but should work for the straightforward case it was designed for.
//

template<typename Fun, std::size_t... Ind>
auto make_Extension_(char const * specCstr, Fun && fun, utility::indices<Ind...>)
    -> Extension<
        typename utility::function_traits<
            typename std::remove_reference<Fun>::type
        >::result_type,
        typename utility::function_traits<
            typename std::remove_reference<Fun>::type
        >::template arg<Ind>...
    >
{
    using Ret = typename utility::function_traits<
        typename std::remove_reference<Fun>::type
    >::result_type;
    return {
        specCstr,
        std::function<
            Ret(
                typename utility::function_traits<
                    typename std::remove_reference<Fun>::type
                >::template arg<Ind>...
            )
        >(fun)
    };
}

template<
    typename Fun,
    typename Indices = utility::make_indices<
        utility::function_traits<
            typename std::remove_reference<Fun>::type
        >::arity
    >
>
auto make_Extension(char const * specCstr, Fun && fun)
    -> decltype(make_Extension_(specCstr, std::forward<Fun>(fun), Indices()))
{
    return make_Extension_(specCstr, std::forward<Fun>(fun), Indices());
}

}

#endif
