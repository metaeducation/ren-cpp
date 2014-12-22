#ifndef RENCPP_COMMON_HPP
#define RENCPP_COMMON_HPP

#include <cstdint>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <tuple>


// http://stackoverflow.com/a/4030983/211160
// Use to indicate a variable is being intentionally not referred to (which
// usually generates a compiler warning)
#ifndef UNUSED
  #define UNUSED(x) ((void)(true ? 0 : ((x), void(), 0)))
#endif

//
// Hopefully not too many of these will creep in; but right now there are some
// by matter of necessity.
//

static_assert(
    sizeof(void *) == sizeof(int32_t),
    "building rencpp binding with non 32-bit pointers..."
    "see evilPointerToInt32Cast in include/rencpp/common.hpp"
);

static_assert(
    sizeof(size_t) == sizeof(int32_t),
    "building rencpp binding with non 32-bit size_t..."
    "see remarks in include/rencpp/common.hpp"
);

inline int32_t evilPointerToInt32Cast(void const * somePointer) {
    return reinterpret_cast<int32_t>(somePointer);
}

template<class T>
inline T evilInt32ToPointerCast(int32_t someInt) {
    static_assert(
        std::is_pointer<T>::value,
        "evilInt32ToPointer cast used on non-ptr"
    );
    return reinterpret_cast<T>(someInt);
}



//
// Not putting const on functions in the C-like exported .h at the moment
// Red/System 1.0 has no const.  It's still nice on the C++ side to keep
// the bookkeeping on constness until the very last moment.
//
template<class T>
inline T * evilMutablePointerCast(T const * somePointer) {
    return const_cast<T*>(somePointer);
}



///
/// GENERAL MAGIC FOR INFERRING TYPES OF LAMBDAS
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

namespace ren {

namespace utility {

template<std::size_t...>
struct indices {};

template<std::size_t N, std::size_t... Ind>
struct make_indices:
    make_indices<N-1, N-1, Ind...>
{};

template<std::size_t... Ind>
struct make_indices<0, Ind...>:
    indices<Ind...>
{};

template<typename T>
struct function_traits:
    function_traits<decltype(&T::operator())>
{};

template<typename C, typename Ret, typename... Args>
struct function_traits<Ret(C::*)(Args...) const>
{
    enum { arity = sizeof...(Args) };

    using result_type = Ret;

    template<std::size_t N>
    using arg = typename std::tuple_element<N, std::tuple<Args...>>::type;
};



///
/// APPLY FUNCTION TO TUPLES
///

//
// This is a clone of std::apply
//

template <typename Func, typename Tuple, std::size_t... Indices>
auto apply_impl(Func && func, Tuple && args, indices<Indices...>)
    -> decltype(
        std::forward<Func>(func)(
            std::get<Indices>(std::forward<Tuple>(args))...)
        )
{
    return std::forward<Func>(func)(
        std::get<Indices>(std::forward<Tuple>(args))...
    );
}

template <
    typename Func,
    typename Tuple,
    typename Indices = make_indices<std::tuple_size<Tuple>::value>
>
auto apply(Func && func, Tuple && args)
    -> decltype(
        apply_impl(
            std::forward<Func>(func),
            std::forward<Tuple>(args),
            Indices {}
        )
    )
{
    return apply_impl(
        std::forward<Func>(func),
        std::forward<Tuple>(args),
        Indices {}
    );
}



///
/// PARAMETER PACKS MANIPULATION
///

//
// This is a clone of the proposed std::type_at
//

template <unsigned N, typename T, typename... R>
struct type_at
{
    using type = typename type_at<N-1, R...>::type;
};

template <typename T, typename... R>
struct type_at<0, T, R...>
{
    using type = T;
};



} // end namespace utility

} // end namespace ren

#endif // end include guard
