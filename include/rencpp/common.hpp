#ifndef RENCPP_COMMON_HPP
#define RENCPP_COMMON_HPP

//
// common.hpp
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

#include <cstdint>
#include <cstddef>
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

    #include <QImage> // For ren::Image

#elif REN_CLASSLIB_QT != 0

    static_assert(false, "Invalid value for REN_CLASSLIB_QT, not 0 or 1.");

#endif



///
/// UNREACHABLE CODE MACRO
///

//
// There are several points where we expect an exception and would like the
// program to halt, but it's not totally certain.  Some ideas in this post
// to consider:
//
//     http://stackoverflow.com/questions/6031819/
//
// Throwing a runtime error will actually stop the code, though.  Which is
// probably the best idea, vs. running ahead anyway.
//

#define UNREACHABLE_CODE() (throw std::runtime_error("Unreachable code"))



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

//
// Enhanced version of Boost function_traits. Here is how:
// * Template arg on size_t (in Boost: arg1_type, arg2_type, etc...)
// * Boost only works up to 10 parameters
// * Boost only works with regular function pointers
// * Boost does not strip the qualifiers
// * We provide helping alias templates
//

template <typename T>
struct function_traits:
    function_traits<decltype(&T::operator())>
{};

template <typename Ret, typename... Args>
struct function_traits<Ret(Args...)>
{
    enum { arity = sizeof...(Args) };

    using result_type = Ret;

    template<std::size_t N>
    using arg = typename type_at<N, Args...>::type;
};

//
// Helpers to convert almost any function type to a simple
// R(Args...) type. It helps making function_traits more
// generic Unfortunately, we still have to list many cases
// by hand in the implementation.
//
//     http://stackoverflow.com/q/27743745/1364752
//

template<typename Ret, typename... Args>
struct function_traits<Ret(*)(Args...)>:
    function_traits<Ret(Args...)>
{};

template<typename C, typename Ret, typename... Args>
struct function_traits<Ret(C::*)(Args...)>:
    function_traits<Ret(Args...)>
{};

template<typename C, typename Ret, typename... Args>
struct function_traits<Ret(C::*)(Args...) const>:
    function_traits<Ret(Args...)>
{};

template<typename C, typename Ret, typename... Args>
struct function_traits<Ret(C::*)(Args...) volatile>:
    function_traits<Ret(Args...)>
{};

template<typename C, typename Ret, typename... Args>
struct function_traits<Ret(C::*)(Args...) const volatile>:
    function_traits<Ret(Args...)>
{};

template<typename T>
struct function_traits<T&>:
    function_traits<T>
{};

template<typename T>
struct function_traits<const T&>:
    function_traits<T>
{};

template<typename T>
struct function_traits<volatile T&>:
    function_traits<T>
{};

template<typename T>
struct function_traits<const volatile T&>:
    function_traits<T>
{};

template<typename T>
struct function_traits<T&&>:
    function_traits<T>
{};

template<typename T>
struct function_traits<const T&&>:
    function_traits<T>
{};

template<typename T>
struct function_traits<volatile T&&>:
    function_traits<T>
{};

template<typename T>
struct function_traits<const volatile T&&>:
    function_traits<T>
{};

//
// Template aliases so that function_traits is easier
// to use. No need to always show the details of this
// template wizardry.
//
// This is C++11, so there is no equivalent for arity.
// There could be some with C++14 variable templates.
//

template<typename T>
using result_type = typename function_traits<T>::result_type;

template<typename T, std::size_t N>
using argument_type = typename function_traits<T>::template arg<N>;

} // end namespace utility

} // end namespace ren

#endif // end include guard
