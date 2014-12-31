#ifndef RENCPP_COMMON_HPP
#define RENCPP_COMMON_HPP

#include <cstdint>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>



///
/// CLASSLIB LIBRAY INCLUSIONS
///

#if REN_CLASSLIB_QT == 1

    // We only use QtCore classes (QString, QByteArray, etc.) in the Ren
    // binding when they are enabled.  So no QtWidgets.

    #include <QString>
    #include <QByteArray> // For const char * of QString (later BINARY!...)

#elif REN_CLASSLIB_QT != 0

    static_assert(false, "Invalid value for REN_CLASSLIB_QT, not 0 or 1.");

#endif


#if REN_CLASSLIB_STD == 1

    // Although we use C++11 classes internally for data structures, there
    // is no express requirement to include std::string.  It is a fairly
    // anemic class in the first place.  So this is the only place we should
    // be including it.

    #include <string>

#elif REN_CLASSLIB_STD != 0

    static_assert(false, "Invalid value for REN_CLASSLIB_STD, not 0 or 1.");

#endif


#if (REN_CLASSLIB_STD == 0) and (REN_CLASSLIB_QT == 0)

    static_assert(false, "Unimplemented feature: no REN_CLASSLIB set"
        " see https://github.com/hostilefork/rencpp/issues/22");

#endif



///
/// BRIDGES TO FIXED-SIZE CONVERSIONS
///

//
// Although 64-bit builds of Rebol do exist, Red is currently focusing on
// 32-bit architectures for the most part.  Hence there are some places
// where pointers can't be used and need to be converted to.  Hopefully not
// too many of these will creep in; but right now there are some by matter
// of necessity and it's good to point them out.
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
/// COMPILE-TIME INTEGER SEQUENCES
///


namespace ren {

namespace utility {

template <std::size_t... Ind>
struct indices {};

template <std::size_t N, std::size_t... Ind>
struct make_indices:
    make_indices<N-1, N-1, Ind...>
{};

template <std::size_t... Ind>
struct make_indices<0, Ind...>:
    indices<Ind...>
{};



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



///
/// FUNCTION TRAITS
///



template <typename T>
struct function_traits:
    function_traits<decltype(&T::operator())>
{};

template <typename C, typename Ret, typename... Args>
struct function_traits<Ret(C::*)(Args...) const>
{
    enum { arity = sizeof...(Args) };

    using result_type = Ret;

    template<std::size_t N>
    using arg = typename type_at<N, Args...>::type;
};


} // end namespace utility

} // end namespace ren

#endif // end include guard
