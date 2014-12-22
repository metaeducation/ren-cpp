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

template<class R, class... Ts>
class Extension : public Value {
protected:
    friend class Value;
    Extension (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isExtension(); }

private:
    typedef std::function<R(Ts...)> FunType;
    typedef std::tuple<Ts...> ParamsType;

    //
    // We need to be able to produce a function value in order to wire this
    // together to call C++ functions from within the runtime.  Rebol natives
    // worked like:
    //
    //     int  (* REBFUN)(REBVAL * ds);
    //
    // In order to make this remotely tractable, I created an extension
    // function type which passes itself as a first parameter.  That way we
    // can map to find the function object we want to use.  Also, it assumes
    // the return convention of R_RET, so the return value cell is to be
    // written into the array under the DS_RETURN address:
    //
    //     void  (* REBFUN)(REBVAL * cpp, REBVAL * ds);
    //
    // If we had more than 96 bits, it might store more for us and keep
    // us from having to look up the shim.  It would be technically possible
    // to poke the bits into the args series (for instance) and then
    // SPEC-OF would give you a series position after that odd value...but
    // that would expose it to users who might look at the head of the series
    //
    // Note that inheriting from std::function would probably be a bad idea:
    //
    //     http://stackoverflow.com/q/27263092/211160
    //

    static std::unordered_map<
        REBCPP, std::tuple<Engine &, size_t, FunType const>
    > table;

    // Function used to create Ts... on the fly and apply a
    // given function to them

    template<typename Func, std::size_t... Indices>
    static auto applyFuncImpl(Func && func,
                              Engine & engine,
                              REBVAL * ds,
                              utility::indices<Indices...>)
        -> decltype(
            std::forward<Func>(func)(
                typename utility::type_at<Indices, Ts...>::type{
                    engine,
                    ds[Indices]
                }...
            )
        )
    {
        return std::forward<Func>(func)(
            typename utility::type_at<Indices, Ts...>::type{
                engine,
                ds[Indices]
            }...
        );
    }

    template <
        typename Func,
        typename Indices = utility::make_indices<sizeof...(Ts)>
    >
    static auto applyFunc(Func && func, Engine & engine, REBVAL * ds)
        -> decltype(
            applyFuncImpl(
                std::forward<Func>(func),
                engine,
                ds,
                Indices {}
            )
        )
    {
        return applyFuncImpl(
            std::forward<Func>(func),
            engine,
            ds,
            Indices {}
        );
}

public:

    Extension (Engine & engine, Block const & spec, FunType const & fun) :
        Value (Dont::Initialize)
    {
        // Reuse the Make_Native function, but set the function pointer to
        // nullptr (we'll overwrite it with our hook in cell's .cpp)

        Make_Native(&cell, VAL_SERIES(&spec.cell), nullptr, REB_CPPHOOK);

        cell.data.func.func.cpp =
            [] (REBVAL * cpp, REBVAL * ds) -> void {

                // Do the table lookup and create references for the pieces.
                // As they are all references; the compiler will optimize this
                // out, but it helps for readability.

                auto & entry = (*table.find(VAL_FUNC_CPP(cpp))).second;

                Engine & engine = std::get<0>(entry);
                size_t & N = std::get<1>(entry);
                FunType const & fun = std::get<2>(entry);

                auto && result = applyFunc(fun, engine, ds);

                // Like R_RET in a native function

                *DS_RETURN = result.cell;
            };

        table.insert(
            make_pair(
                cell.data.func.func.cpp,
                std::forward_as_tuple(engine, sizeof...(Ts), fun)
            )
        );

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



};


template<class R, class... Ts>
std::unordered_map<
    REBCPP,
    std::tuple<Engine &, size_t, typename Extension<R, Ts...>::FunType const>
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
