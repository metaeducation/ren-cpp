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
class Extension : public Value {
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
        REBFUN, TableEntry
    > table;

    static void tableAdd(REBFUN shim, TableEntry && entry) {
        using internal::extensionTablesMutex;

        std::lock_guard<std::mutex> lock {extensionTablesMutex};

        table.insert(std::make_pair(shim, entry));
    }

    static TableEntry tableFindAndRemove(REBFUN shim) {
        using internal::extensionTablesMutex;

        std::lock_guard<std::mutex> lock {extensionTablesMutex};

        auto it = table.find(shim);

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
        REBVAL * ds,
        utility::indices<Indices...>
    )
        -> decltype(
            std::forward<FunType const &>(fun)(
                typename utility::type_at<Indices, Ts...>::type{
                    engine,
                    *D_ARG(Indices + 1)
                }...
            )
        )
    {
        return std::forward<FunType const &>(fun)(
            typename utility::type_at<Indices, Ts...>::type{
                engine,
                *D_ARG(Indices + 1)
            }...
        );
    }

    template <
        typename Indices = utility::make_indices<sizeof...(Ts)>
    >
    static auto applyFun(FunType const & fun, Engine & engine, REBVAL * ds)
        -> decltype(
            applyFunImpl(
                std::forward<FunType const &>(fun),
                engine,
                ds,
                Indices {}
            )
        )
    {
        return applyFunImpl(
            std::forward<FunType const &>(fun),
            engine,
            ds,
            Indices {}
        );
}

public:

    Extension (Engine & engine, Block const & spec, FunType const & fun) :
        Value (Dont::Initialize)
    {
        REBFUN const & shim  =
            [] (REBVAL * ds) -> int {

                // Do the table lookup.  Rebol places the function value on
                // the stack between where we store the return value and
                // the arguments.  See DSF_XXX for the other definitions.

                // We only need to do this lookup once.  Though the static
                // variable initialization is thread-safe, the table access
                // itself is not...so it is protected via mutex.

                static TableEntry entry = tableFindAndRemove(
                    VAL_FUNC_CODE(DSF_FUNC(ds - DS_Base))
                );

                auto && result = applyFun(entry.fun, entry.engine, ds);

                *DS_RETURN = result.cell;
                return R_RET;
            };

        // Insert the shim into the mapping table so it can find itself while
        // the shim code is running.  Note that the tableAdd code has
        // to be thread-safe in case two threads try to modify the global
        // table at the same time...

        tableAdd(shim, TableEntry {engine, fun});

        // Reuse the Make_Native function to set the values up

        Make_Native(&cell, VAL_SERIES(&spec.cell), shim, REB_NATIVE);

        finishInit(engine.getHandle());
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
    REBFUN,
    typename Extension<R, Ts...>::TableEntry
> Extension<R, Ts...>::table;



///
/// UGLY LAMBDA INFERENCE NIGHTMARE
///


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
