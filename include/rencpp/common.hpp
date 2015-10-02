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



//
// CLASSLIB LIBRAY INCLUSIONS
//

#if REN_CLASSLIB_QT == 1

    // We only use QtCore classes (QString, QByteArray, etc.) in the Ren
    // binding when they are enabled.  So no QtWidgets.

    #include <QString>
    #include <QByteArray> // For const char * of QString (later BINARY!...)

    #include <QImage> // For ren::Image

#elif REN_CLASSLIB_QT != 0

    static_assert(false, "Invalid value for REN_CLASSLIB_QT, not 0 or 1.");

#endif



//
// UNREACHABLE CODE MACRO
//

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



//
// REN::OPTIONAL (std::(experimental)::optional, compact_optional?)
//
// In the middle of Ren/C++'s design timeline, the "UNSET!" Rebol type was
// altered from being an "ordinary" value type.  It became an "optional" state
// that happened only during expression evaluation.  Though it still "exists"
// as a cell state (in the sense of an implementation detail for efficiency)
// it could no longer be persisted in places like blocks.  So there was no
// such thing as `[10 20 #[unset!]]`...for instance.
//
// Conceptually this made it so that what had been `ren::Unset` was now more
// like the disengaged state of a `std::optional<AnyType>`.  The transition
// brought semantic benefit and clarity to the binding.  But `std::optional`
// is not in C++11, was delayed from being included in C++14, and pushed to
// a library called "Fundamentals-TS".  And Ren/C++ still was targeting
// the goal of working in stock C++11 compilers.
//
// To deal with the lack of ubiquitous availability of an approved optional
// implementation, class methods target optional under the ren namespace as
// `ren::optional`.  At time of writing, this aliases an implementation in
// the %include/ directory of the distribution.  That is a direct snapshot
// of the draft spec which was borrowed by the C++ standards committee:
//
//      https://github.com/akrzemi1/Optional
//
// `ren::optional` could be re-aliased to `std::optional` on compilers
// that provide the library.  However, the fully generic optional adds extra
// storage use (vs. reserving a value in the contained type).  Since both
// Rebol and Red can encode "unsetness" in a cell header, the correct answer
// is to use something like Andrzej Krzemienski's other `compact_optional`:
//
//     https://akrzemi1.wordpress.com/2015/07/15/efficient-optional-values/
//
// Because this optional may not have quite the same interface as
// `std::optional`, it makes sense to have the system use `ren::optional`.
// Over the long term this class will match the storage efficiency of its
// contained `ren::AnyType` (currently using a generic optional just in
// order to ensure the mechanics are working).
//
// It may be--however--that `std::optional<AnyType>` could have a template
// specialization which gave the benefits of `compact_optional`:
//
//      http://blog.hostilefork.com/template-specialize-optional-or-not/
//
// Were such a specialization to be provided, then `ren::optional` would
// be able to be equivalent to (and compatible with) `std::optional`,
// even if its implementation was like `compact_optional` under the hood.
//

#include "optional/optional.hpp"

namespace ren {

using nullopt_t = std::experimental::nullopt_t;

constexpr ren::nullopt_t nullopt{
    std::experimental::nullopt_t::init{}
};

//
// There is a `bad_optional_access` exception thrown when disengaged optionals
// are dereferenced.  However, this exception has no parameters.  There's no
// awareness of a name or line number to identify where the problem happened:
//
//      optional<Value> foo;
//
//      if (*foo)
//          std::cout << "The exception won't/can't indicate foo."; }
//
// One solution for getting better errors would be to use a level of indirect
// through a WORD!.  Unlike a C++ variable, a word has a name which can be
// indicated at runtime:
//
//      /* pending feature--binding and expandability settings */
//      Object context {ren::Object::Expandable};
//      Word foo {"foo-word", context, ren::Bind::Add};
//
//      if (foo.get())
//          std::cout << "Exception *can* indicate foo-word";
//
// However, in the early migration of optional there are a lot of instances
// of calling `static_cast<Type>(*runtime("code here"))`.  This assumes that
// the code will return a result of Type, and does the dereference.  Both
// are points of failure that do a C++ `throw`, and *can* be caught and
// delivered "gracefully".  The message won't be all that different from
// getting `** Script Error: ...` but there will be no trace or variable
// name to help find it.
//
// To help with debugging bad optional dereferences, the debug build wraps
// optional with methods that `assert()` instead of `throw`.  This also helps
// to emphasize that bad optional dereferencing should not be used as a
// "feature", as the error will be uninformative to the user.
//

using bad_optional_access = std::experimental::bad_optional_access;

#ifndef NDEBUG
    template<typename T>
    using optional = std::experimental::optional<T>;
#else
    template<typename T>
    class optional : public std::experimental::optional<T> {
    public:
        using std::experimental::optional::optional;

        // Assertions inserted in debug builds for localizing errors easily

        constexpr T* operator->() const {
            assert(*this != nullptr);
            return optional::operator->();
        }

        constexpr T& operator*() const {
            assert(*this != nullptr);
            return optional::operator*();
        }

        constexpr T& value() const {
            assert(*this != nullptr);
            return optional::value();
        }
    };
#endif

namespace utility {

// It can be desirable to do SFINAE against an optional and extract its
// type, this helper class does that:
//
//      http://stackoverflow.com/questions/32859415/

template <typename T>
struct extract_optional;

template <typename T>
struct extract_optional<std::experimental::optional<T>>
{
    using type = T;
};

template <typename T>
using extract_optional_t = typename extract_optional<T>::type;

} // end namespace ren::utility

} // end namespace ren



namespace ren {

namespace utility {

//
// COMPILE-TIME INTEGER SEQUENCES
//

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



//
// PARAMETER PACKS MANIPULATION
//

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



//
// FUNCTION TRAITS
//

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
