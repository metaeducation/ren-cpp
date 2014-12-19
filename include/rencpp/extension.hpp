#ifndef RENCPP_EXTENSION_HPP
#define RENCPP_EXTENSION_HPP

#include <functional>
#include <utility>
#include <unordered_map>

#include "values.hpp"
#include "engine.hpp"
#include "runtime.hpp"

namespace ren {

///
/// IN-PROGRESS DESIGN ON MAKING RED EXTENSION FUNCTIONS
///

template<typename Ret, typename... Args>
class Foo
{
private:
    std::function<Ret(Args...)> _func;

public:
    Foo(const std::function<Ret(Args...)>& func):
        _func(func)
    {}

    auto operator()(Args... args)
        -> Ret
    {
        return _func(args...);
    }
};



//
// Initial attempt to try and understand how one might use modern variadic
// templates to automatically pack up a bit of C++ code to run as the body of
// a function provided to Red.  I'm not sure what the limits of such an
// approach are as I've never tried, so I'll just see what happens.
//
// First plan was to try inheriting from std::function, but add some more
// general magic to pick apart the parameter types.  Inheriting is probably
// not the best idea; but it's a start:
//
//     http://stackoverflow.com/q/27263092/211160
//


// There may be ways of making the spec block automatically, but naming
// the parameters would be difficult.  A version that took a single
// argument and just called them arg1 arg2 etc and built the type
// strings may be possible in the future.

template<class R, class... Ts>
class Extension : public Value {
protected:
    friend class Value;
    Extension (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isExtension(); }

private:
    typedef std::function<R(Ts...)> FunType;
    typedef std::tuple<Ts...> ParamsType;

    // If we had more than 96 bits, it might store more for us and keep
    // us from having to look up the shim.

    static std::unordered_map<
        REBCPP, std::tuple<Engine *, size_t, FunType>
    > table;

public:
    template<int ...S>
    static R callFunc(
        FunType const & fun,
        ParamsType const & params,
        utility::seq<S...>
    ) {
        return fun(std::get<S>(params) ...);
    }

    Extension (Engine & engine, Block const & spec, FunType const & fun) :
        Value (Dont::Initialize)
    {
        // Reuse the Make_Native function, but set the function pointer to
        // nullptr (we'll overwrite it with our hook in cell's .cpp)

        Make_Native(&cell, VAL_SERIES(&spec.cell), nullptr, REB_CPPHOOK);

        cell.data.func.func.cpp =
            [] (REBVAL * cpp, REBVAL * ds) -> void {
            // Assumption is you will only ever register an extension with
            // one engine (for the moment).  This gets trickier if you can
            // do that.  Of course, as long as there's only one engine this
            // will not be possible anyway, so the sloppy assumption isn't
            // so bad.

            auto it = table.find(VAL_FUNC_CPP(cpp));

            // we need to proxy this array of cells in so that it actually
            // is an array of values; this can't be done at compile time
            // unfortunately.  We have to take for granted that the number
            // of function parameters is equal to what we expect.

            size_t N = std::get<1>((*it).second);

            std::tuple<Ts...> params;
            /*
            Value arr[N];

            for (size_t index = 0; index < 2; index++) {
                arr[index].cell = *D_ARG(index + 1);
                arr[index].finishInit(std::get<0>((*it).second)->getHandle());
                arr[index].trackLifetime();
            }*/


            UNUSED(ds);
            UNUSED(cpp);
            R result = callFunc(
                std::get<2>((*it).second),
                params,
                typename utility::gens<sizeof...(Ts)>::type()
            );

            // Like R_RET in a native function

            *DS_RETURN = result.cell;
        };

        table[cell.data.func.func.cpp]
            = std::make_tuple(&engine, sizeof...(Ts), fun);
        finishInit(engine.getHandle());
    }

    Extension (Block spec, FunType const & fun) :
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

#ifndef NDEBUG
    void addRefDebug();
#endif

    // We need to be able to produce a function value in order to wire this
    // together to call C++ functions from within the runtime.  Rebol natives
    // worked like:
    //
    //     int  (*REBFUN)(REBVAL *ds);
    //
    // In order to make this remotely tractable, I created an extension
    // function type which passes itself as a first parameter.  That way we
    // can map to find the function object we want to use.
    //
    // Clearly that's not what we get in, so we have to make one of those.
    // It is dependent on the stack.  So what you get is:
    //
    // enum {
    //    R_RET = 0,
    //    R_TOS,
    //    R_TOS1,
    //    R_NONE,
    //    R_UNSET,
    //    R_TRUE,
    //    R_FALSE,
    //    R_ARG1,
    //    R_ARG2,
    //    R_ARG3
    // };
    //
    // So to shim this together we need to write that function.
};


template<class R, class... Ts>
std::unordered_map<
    REBCPP, std::tuple<Engine *, size_t, typename Extension<R, Ts...>::FunType>
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



// Extending via object is probably a popular thing, I think a good notation
// for that is desirable.

class ExtensionObject {
public:
    ExtensionObject(
        std::initializer_list<
            std::pair<internal::Loadable, internal::Loadable>
        > pairs
    ) {
        UNUSED(pairs);
    }
};

}

#endif
