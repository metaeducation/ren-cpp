#ifndef RENCPP_VALUES_HPP
#define RENCPP_VALUES_HPP

//
// values.hpp
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

#include <iostream>
#include <cstdint>
#include <cassert>
#include <utility> // std::forward

#include <atomic>
#include <type_traits>
#include <array>

#include <typeinfo> // std::bad_cast

#include "common.hpp"


extern "C" {
#include "hooks.h"
}


namespace ren {


///
/// FORWARD DEFINITIONS FOR CLASSES IN RUNTIME.HPP
///

//
// Abstractly speaking the runtime is a "separate thing" from the basic type
// list (which would be more akin to "Ren"):
//
//    https://github.com/humanistic/REN
//
// Given how long header files get, it's nice to break things up somehow;
// and a delineation of runtime loading and evaluation seems a good breaking
// point.  Yet although a bare-bones interface where the values themselves
// were "dead data" and you always had to pass them to the appropriate
// runtime...that's a bit lame.  You couldn't do something like:
//
//     auto print = ren::Word("print");
//     print(10 + 20);
//
// Hence the ren::Context (or "Context") has to be known to the values
// that operate with them, to expose "generalized apply" and other functions.
//


class Context;

class Engine;


namespace internal {
    //
    // Note that you can't forward declare objects in nested scopes:
    //
    //    http://stackoverflow.com/q/951234/211160
    //
    // However by putting Loadable inside a "::internal" namespace it might
    // seem more obvious that users shouldn't be creating instances of it
    // themselves, despite the requirement of public construction functions.
    //
    class Loadable;

#ifndef REN_RUNTIME
#elif REN_RUNTIME == REN_RUNTIME_RED
    class FakeRedHooks;
#elif REN_RUNTIME == REN_RUNTIME_REBOL
    class RebolHooks;
#else
    static_assert(false, "Invalid runtime setting")
#endif

    template <class R, class... Ts> class FunctionGenerator;
}

struct none_t;

struct unset_t;



///
/// CAST EXCEPTION
///

//
// Although we can define most of the exceptions in exceptions.hpp, this one
// is thrown by Value itself in templated code and needs to be here
//

class bad_value_cast : public std::bad_cast {
private:
    std::string whatString;

public:
    bad_value_cast (std::string const & whatString) :
        whatString (whatString)
    {
    }

    char const * what() const noexcept override {
        return whatString.c_str();
    }
};



///
/// VALUE BASE CLASS
///

//
// In the encapsulation as written, we pay additional costs for a reference
// count, plus a handle to which runtime instance the value belongs to.  While
// it may seem that adding another 64-bits is a lot to add to a cell...remember
// this is only for values that get bridged.  A series with a million elements
// in it is not suddenly costing 64-bits per element; the internals are
// managing the 128-bit cells and the series reference itself is the only
// one value that needs the overhead in the binding.
//

class Value {
protected:
    friend class Runtime; // cell
    friend class Engine; // Value::Dont::Initialize
    friend class Context; // Value::Dont::Initialize
    friend class Series; // Value::Dont::Initialize for dereference
    friend class AnyBlock; // Value::Dont::Initialize for dereference
    friend class AnyString;

public: // temporary for the lambda in function, find better way
    RenCell cell;

#ifndef REN_RUNTIME
#elif REN_RUNTIME == REN_RUNTIME_RED
    friend class FakeRedHooks;
#elif REN_RUNTIME == REN_RUNTIME_REBOL
    friend class internal::RebolHooks;
#else
    static_assert(false, "Invalid runtime setting")
#endif

    //
    // We must reference count many types.  Note that this reference count
    // does not introduce problems of cycles, because we never put references
    // to ren::Values into a ren::Series.
    //
    // Reference counts aren't copy constructible (by design), which means
    // hat when a series value gets wrapped we do a `new` on the atomic.  On
    // the plus side: copying that value around won't make any more of them,
    // and non-series values don't have them at all.
    //
    using RefcountType = std::atomic<unsigned int>;
    RefcountType * refcountPtr;

    //
    // While I realize that "adding a few more bytes here and there" in Red
    // and Rebol culture is something that is considered a problem, this is
    // a binding layer.  It does not store a reference for each value in a
    // series...if your series is a million elements long and you hold a
    // reference to that... you aren't paying for the added storage of a
    // bound reference for each element.
    //
    // Perhaps a raw cell based interface has applications, but really, isn't
    // the right "raw cell based interface" just to program in Red/System?
    //
    // Anyway, the API is set up in such a way that we could have values
    // coming from different engines, and there is no enforcement that the
    // internals remember which handles go with which engine.  This little
    // bit of per-value bookkeeping helps keep things straight when we
    // release the value for GC.
    //

    RenEngineHandle origin;


    //
    // At first the only user-facing constructor that was exposed directly
    // from Value was the default constructor, used to make an UNSET!
    //
    //     ren::Value something;
    //
    // But support for other construction types directly as Value has been
    // incorporated.  For the rationale, see:
    //
    //     https://github.com/hostilefork/rencpp/issues/2
    //
public:
    Value (Engine & engine);
    Value ();

    // Same as Value() but sometimes more clear to write ren::unset
    Value (unset_t const &) : Value() {}


    bool isUnset() const;


    //
    // There is a default constructor, and it initializes the RenCell to be
    // a constructed value of type UNSET!
    //
    // BUT as an implementation performance detail, if the default constructor
    // is bothering to initialize the 128 bits, what if a derived class
    // wants to do all its initialization in the constructor body?  Perhaps
    // the code uses code that would mean taking addresses of
    // temporaries and such, so it needs to construct the base class somehow.
    //
    //     DerivedType (...) : Value(Dont::Initiailize)
    //     {
    //        // derived class code that can cope with that
    //     }
    //
    // Hadn't seen any precedent for this, but once I came up with it I
    // did notice it in Qt::Uninitialized...
    //
    //     https://qt.gitorious.org/qt/icefox/commit/fbe0edc
    //
    // Call finishInit once the cell bits have been properly set up, so
    // that any tracking/refcounting/etc. can be added.
    //
    // We also provide a construct template function which allows friend
    // classes of value to get access to this constructor on all derived
    // classes of Value (which have friended Value)
    //

protected:
    enum class Dont {Initialize};
    Value (Dont const &);

    void finishInit(RenEngineHandle engine);

    template<
        class T,
        typename = typename std::enable_if<
            std::is_base_of<Value, T>::value
        >::type
    >
    static T construct (Dont const &) {
        return T {Dont::Initialize};
    }


    //
    // The value-from-cell constructor does not check the bits, and all cell
    // based constructors are not expected to either.  You trust they were
    // set up correctly OR if they are not, then the cast operator is tasked
    // with checking their invalidity and throwing an exception
    //
    // Again, we provide a catapulting "construct" function to give value's
    // friends access to this construction for any derived class.
    //
protected:
    template <class R, class... Ts>
    friend class internal::FunctionGenerator;

    explicit Value (RenEngineHandle engine, RenCell const & cell) {
        this->cell = cell;
        finishInit(engine);
    }

    template<
        class T,
        typename = typename std::enable_if<
            std::is_base_of<Value, T>::value
        >::type
    >
    static T construct (RenEngineHandle engine, RenCell const & cell) {
        T result {Dont::Initialize};
        result.cell = cell;
        result.finishInit(engine);
        return result;
    }

public:
    //
    // Constructing from nullptr is ugly, but it's nice to be able to just
    // assign from "none".
    //
    // https://github.com/hostilefork/rencpp/issues/3
    //

    bool isNone() const;

    // actually never provide this; this is a disabling.  = delete is
    // apparently not a "better" way of doing such a disablement.

    // disabling doesn't work in clang, bad idea?
    /* Value (nullptr_t) = delete; */
    /* Value (nullptr_t); */

    Value (Engine & engine, none_t const &);
    Value (none_t const &);


public:
    //
    // We *explicitly* coerce to operator bool.  This only does the conversion
    // automatically under contexts like if(); the C++11 replacement for
    // "safe bool idiom" of if (!!x).  As with UNSET! in Rebol/Red, it will
    // throw an exception if you try to use it on a value that turns out
    // to be UNSET!
    //
    // http://stackoverflow.com/questions/6242768/
    //
    Value (Engine & engine, bool const & b);
    Value (bool const & b);

    bool isLogic() const;

    bool isTrue() const;

    bool isFalse() const;

    // http://stackoverflow.com/q/6242768/211160
    explicit operator bool() const;


public:
    Value (Engine & engine, char const & c);
    Value (char const & c);

    Value (Engine & engine, wchar_t const & c);
    Value (wchar_t const & c);

    bool isCharacter() const;


public:
    Value (Engine & engine, int const & i);
    Value (int const & i);

    bool isInteger() const;


public:
    // Literals are double by default unless you suffix with "f"
    //     http://stackoverflow.com/a/4353788/211160
    Value (double const & d);
    Value (Engine & engine, double const & d);

    bool isFloat() const;


public:
    // The C and C++ date and time routines all seem about counting since
    // epoch or in dealing with time measuring of relative intervals on
    // a CPU clock.  They are not very serviceable for working with arbitrary
    // dates that could reach far back into the past and general calendaring
    // math.  The closest analogue seems to be:
    //
    //     http://stackoverflow.com/questions/6730422/
    //
    // Hesitant to put in a dependency just for that.  If you want to extract
    // information from the date you can do that with calls into the
    // evaluator.

    bool isDate() const;

    bool isTime() const;

public:
    bool isWord(RenCell * = nullptr) const;

    bool isSetWord(RenCell * = nullptr) const;

    bool isGetWord(RenCell * = nullptr) const;

    bool isLitWord(RenCell * = nullptr) const;

    bool isRefinement(RenCell * = nullptr) const;

    bool isIssue(RenCell * = nullptr) const;

    bool isAnyWord() const;


public:
    bool isBlock(RenCell * = nullptr) const;

    bool isParen(RenCell * = nullptr) const;

    bool isPath(RenCell * = nullptr) const;

    bool isGetPath(RenCell * = nullptr) const;

    bool isSetPath(RenCell * = nullptr) const;

    bool isLitPath(RenCell * = nullptr) const;

    bool isAnyBlock() const;

    bool isAnyString() const;

    bool isSeries() const;

public:
    bool isString(RenCell * = nullptr) const;

    bool isTag(RenCell * = nullptr) const;

public:
    bool isFunction() const;

    bool isError() const;

protected:
    bool needsRefcount() const;

private:
    inline void releaseRefIfNecessary() {
        // refcount is nullptr if it's not an refcountable type, or if it was
        // a refcountable type and we used the move constructor to empty i
        if (refcountPtr && !--(*refcountPtr)) {
            delete refcountPtr;
            if (
                ::RenReleaseCells(origin, &this->cell, 1, sizeof(Value))
                != REN_SUCCESS
            ) {
                throw std::runtime_error(
                    "Refcounting problem reported by the Red binding hook"
                );
            }
        }
    }

public:
    //
    // Copy construction must make a new copy of the 128 bits in the cell, as
    // well as add to the refcount (if it exists)
    //
    // Wondering what might happen if while you are initializing, another
    // thread decrements the refcount and it goes to zero before you can
    // finish?  Don't worry - you know at least one reference on this thread
    // will be keeping it alive (the one that you are copying!)
    //
    Value (Value const & other) :
        cell (other.cell),
        refcountPtr (other.refcountPtr),
        origin (other.origin)
    {
        if (refcountPtr)
            (*refcountPtr)++;
    }

    //
    // Move construction only has to copy the bits; it can take the other's
    // refcount pointer without doing an atomic increment.
    //
    // User-defined move constructors should not throw exceptions.  We
    // trust the C++ type system here.  You can move a String into an
    // AnySeries but not vice-versa.
    //
    Value (Value && other) :
        cell (other.cell),
        refcountPtr (other.refcountPtr),
        origin (other.origin)
    {
        // Technically speaking, we don't have to null out the other's
        // refcount and runtime handle.  But it's worth it for the safety,
        // because sometimes values that have been moved *do* get used on
        // accident after the move.

        other.refcountPtr = nullptr;
        other.origin = REN_ENGINE_HANDLE_INVALID;
    }

    Value & operator=(Value const & other) {
        // we're about to overwrite the content, so release the reference
        // on the content we had
        releaseRefIfNecessary();

        cell = other.cell;
        refcountPtr = other.refcountPtr;
        if (refcountPtr) {
            (*refcountPtr)++;
        }
        return *this;
    }

public:
    ~Value () {
        releaseRefIfNecessary();
    }


    //
    // Equality and Inequality
    //
    // Note the semantics here are about comparing the values as equal in
    // the sense of value equality.  For why we do not do bit equality, see:
    //
    //     https://github.com/hostilefork/rencpp/issues/25
    //
    // Having implicit constructors for C++ native types is more valuable
    // than overloading == and != which is especially true given that C++
    // has a different meaning for == than Rebol/Red.  Trying to make ==
    // act like "isSameAs" causes ambiguity.
    //
public:
    bool isEqualTo(Value const & other) const;
    bool isSameAs(Value const & other) const;


public:
    friend std::ostream & operator<<(std::ostream & os, Value const & value);

    // If you explicitly ask for it, all ren::Value types can be static_cast
    // to a std::string.  Internally it uses the runtime::form
    // which is (ideally) bound to the native form and cannot be changed.
    // Only the string type allows implicit casting, though it provides the
    // same answer.

#if REN_CLASSLIB_STD
    explicit operator std::string () const;
#endif

#if REN_CLASSLIB_QT
    explicit operator QString () const;
#endif

    // The strategy is that ren::Values are not evaluated by default when
    // being passed around and used in C++.  For why this has to be the way
    // it is, see:
    //
    //     https://github.com/hostilefork/rencpp/issues/4
    //
    // One could use functions to "get" the value of a word, or do operator
    // overloading so that *x means "dereference" (as in std::optional).  But
    // using the function apply syntax offers a convenient possibility.  Then
    // x(y, z) can have the meaning of "execute as if you were DOing a block
    // where x were spliced at the head of it with y and z following.
    //
    // It's sort of DO-like and kind of APPLY like if it's a function value.
    // I'm calling it apply here for "generalized apply".  So far it's
    // the best generalization I have come up with which puts a nice syntax on
    // do (which is a C++ keyword and not available)
    //
protected:
    Value apply(
        RenCell loadables[],
        size_t numLoadables,
        Context * context
    ) const;

public:
    Value apply(
        std::initializer_list<internal::Loadable> loadables,
        Context * context = nullptr
    ) const;

    template <typename... Ts>
    inline Value apply(Ts const &... args) const {
        return apply(std::initializer_list<internal::Loadable>{args...});
    }

    template <typename... Ts>
    inline Value operator()(Ts... args) const {
        return apply(std::forward<Ts>(args)...);
    }

    // The explicit (and throwing) cast operators are defined via template
    // for any casts that don't already have valid paths in the hierarchy.
    // So although a String is a Series and can upcast to it, by default
    // there is no way to turn a Series into a string.  This template
    // generation creates those missing *explicit* cast operators, but they
    // will throw an exception if you were wrong.  So don't be wrong, unless
    // you're prepared to catch exceptions or have your program crash.  Test
    // the type before the conversion first.
    //
    // Surprised this is possible?  I was too:
    //
    //     http://stackoverflow.com/q/27436039/211160
    //
public:
    template <
        class T,
        typename = typename std::enable_if<
            std::is_base_of<Value, T>::value
            and not std::is_same<Value, T>::value
        >::type
    >
    explicit operator T () const
    {
        // Here's the tough bit.  How do we throw exceptions on all the right
        // cases?  Each class needs a checker for the bits.  So it constructs
        // the instance, with the bits, but then throws if it's bad...and it's
        // not virtual.
        T result (Dont::Initialize);
        result.cell = cell;
        result.finishInit(origin);

        if (not result.isValid())
            throw bad_value_cast("Invalid cast");

        return result;
    }
};



///
/// NONE CONSTRUCTION FUNCTION
///

//
// Can't use an extern variable of None statically initialized, because the
// engine handle wouldn't be set...and static constructor ordering is
// indeterminate anyway.
//
// Should you be able to "apply" a none directly, as none(arg1, arg2) etc?
// It seems okay to disallow it.  But if you want that (just so it can fail
// if you ever gave it parameters, for the sake of completeness) there'd
// have to be an operator() here.
//

struct none_t
{
  struct init {};
  constexpr none_t(init) {}
};
constexpr none_t none {none_t::init{}};


struct unset_t
{
  struct init {};
  constexpr unset_t(init) {}
};
constexpr unset_t unset {unset_t::init{}};



///
/// LEAF VALUE CLASS PROXIES
///

//
// These classes inherit from Value, without inheriting Value's constructors.
// Since they inherited from Value you can pass them into slots that are
// expecting a Value.  However, a static_cast<> must be used if you want
// to go the other way (which may throw an exception on a bad cast).
//
// At minimum, each derived class must provide these methods:
//
// protected:
//    friend class Value;
//    Foo (Dont const &) : Value (Dont::Initialize) {}
//    inline bool isValid() const { return ...; }
//
// These are needed by the base class casting operator in Value, which has
// to be able to ask if the result of a cast is a valid instance for the
// specific or general type.  We're trying not to pay for virtual dispatch
// here, which is why it's so weird...and we don't want to expose this
// internal to users, which is why Value needs to be a friend.
//


class Unset final : public Value {
protected:
    friend class Value;
    Unset (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isUnset(); }

public:
    Unset () :
        Value ()
    {
    }

    explicit operator bool() const = delete;
};


class None final : public Value {
protected:
    friend class Value;
    None (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isUnset(); }

public:
    explicit None (Engine & engine);
    explicit None ();
};


class Logic final : public Value {
protected:
    friend class Value;
    Logic (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isLogic(); }

public:
    // Narrow the construction?
    // https://github.com/hostilefork/rencpp/issues/24
    Logic (bool const & b) :
        Value (b)
    {
    }

    operator bool () const;
};


class Character final : public Value {
protected:
    friend class Value;
    Character (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isCharacter(); }

public:
    Character (char const & c) :
        Value (c)
    {
    }

    Character (wchar_t const & wc) :
        Value (wc)
    {
    }


    // Characters represent codepoints.  These conversion operators are for
    // convenience, but note that they may throw.

    operator char () const;
    operator wchar_t () const;
    operator int () const;

    // How to expose UTF8 encoding?
};


class Integer final : public Value {
protected:
    friend class Value;
    Integer (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isInteger(); }

public:
    Integer (int const & i) :
        Value (i)
    {
    }

    operator int () const;
};


class Float final : public Value {
protected:
    friend class Value;
    Float (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isFloat(); }

public:
    Float (double const & d) :
        Value (d)
    {
    }

    operator double () const;
};


class Date final : public Value {
protected:
    friend class Value;
    Date (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isDate(); }

public:
    explicit Date (std::string const & str);
};



///
/// GENERIC CATEGORY TYPES
///

//
// These are base classes that represent "categories" of types which can be
// checked at compile time in the inheritance hierarchy.  The only way you
// can get an instance of one of these types is via an already existing
// instance.  So you can assign a SetWord to an AnyWord, but you can't
// construct an AnyWord from scratch.  (What type of word would it be?)
//
// They use an unusual-looking pattern, which is to pass around a pointer
// to a member function.  This pointer takes the place of having to expose
// an enumerated type in the interface that corresponds numbers to types.
//
// One trick is that the validMem(ber)F(u)n(c)tion can not only test if a
// type is valid for its instance or category, but if you pass them a
// pointer to a cell they can write their type signature into that cell.
// It's actually rather cool, I think.
//

class AnyWord : public Value {
protected:
    friend class Value;
    AnyWord (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isAnyWord(); }

protected:
    explicit AnyWord (
        char const * cstr,
        bool (Value::*validMemFn)(RenCell *) const,
        Context * context = nullptr
    );


#if REN_CLASSLIB_STD
    explicit AnyWord (
        std::string const & str,
        bool (Value::*validMemFn)(RenCell *) const,
        Context * context = nullptr
    ) :
        AnyWord (str.c_str(), validMemFn, context)
    {
    }
#endif


#if REN_CLASSLIB_QT
    explicit AnyWord (
        QString const & str,
        bool (Value::*validMemFn)(RenCell *) const,
        Context * context = nullptr
    );
#endif

public:
    template <
        class T =
#if REN_CLASSLIB_STD
            std::string
#elif REN_CLASSLIB_QT
            QString
#else
    static_assert(false, "https://github.com/hostilefork/rencpp/issues/22");
#endif
    >
    T spellingOf() const {
        throw std::runtime_error("Unspecialized version of spellingOf called");
    }

#if REN_CLASSLIB_STD
    std::string spellingOf_STD() const;
#endif

#if REN_CLASSLIB_QT
    QString spellingOf_QT() const;
#endif

};

// http://stackoverflow.com/a/3052604/211160

#if REN_CLASSLIB_STD
template<>
inline std::string AnyWord::spellingOf<std::string>() const {
    return spellingOf_STD();
}
#endif

#if REN_CLASSLIB_QT
template<>
inline QString AnyWord::spellingOf<QString>() const {
    return spellingOf_QT();
}
#endif



class Series : public Value {
protected:
    friend class Value;
    Series (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isSeries(); }

    //
    // If you wonder why C++ would need a separate iterator type for a Series
    // instead of doing as Rebol does and just using a Series, see this:
    //
    //    https://github.com/hostilefork/rencpp/issues/25
    //
public:
    class iterator {
        friend class Series;
        Value state; // it's a Series, but that's incomplete type
        mutable Value valForArrow;
        iterator (Series const & state) :
            state (state),
            valForArrow (Dont::Initialize)
        {
        }

    public:
        iterator & operator++(); // prefix
        iterator & operator--();

        iterator operator++(int); // int means "postfix"
        iterator operator--(int);

        bool operator==(iterator const & other)
            { return state.isSameAs(other.state); }
        bool operator!=(iterator const & other)
            { return not state.isSameAs(other.state); }

        Value operator * () const;
        Value * operator-> () const;
        explicit operator Series () const;
    };

    friend class iterator;

    iterator begin();
    iterator end();
};



class AnyString : public Series
{
protected:
    friend class Value;
    AnyString (Dont const &) : Series (Dont::Initialize) {}
    inline bool isValid() const { return isAnyString(); }

protected:
    AnyString(
        char const * cstr,
        bool (Value::*validMemFn)(RenCell *) const,
        Engine * engine = nullptr
    );

#if REN_CLASSLIB_STD
    AnyString (
        std::string const & str,
        bool (Value::*validMemFn)(RenCell *) const,
        Engine * engine = nullptr
    );
#endif

#if REN_CLASSLIB_QT
    AnyString (
        QString const & str,
        bool (Value::*validMemFn)(RenCell *) const,
        Engine * engine = nullptr
    );
#endif

public:
    // This lets you pass ren::String to anything that expected a std::string
    // and do so implicitly, which may or may not be a great idea.  See:
    //
    //     https://github.com/hostilefork/rencpp/issues/6

#if REN_CLASSLIB_STD
    operator std::string () const {
        Value const & thisValue = *this;
        return static_cast<std::string>(thisValue);
    }
#endif


#if REN_CLASSLIB_QT
    operator QString () const {
        Value const & thisValue = *this;
        return static_cast<QString>(thisValue);
    }
#endif


public:
    class iterator {
        friend class AnyString;
        Value state; // It's an AnyString but that's incomplete type
        mutable Character chForArrow;
        iterator (AnyString const & state) :
            state (state),
            chForArrow (Value::construct<Character>(Dont::Initialize))
        {
        }

    public:
        iterator & operator++(); // prefix
        iterator & operator--();

        iterator operator++(int); // int means postfix
        iterator operator--(int);

        bool operator==(iterator const & other)
            { return state.isSameAs(other.state); }
        bool operator!=(iterator const & other)
            { return state.isSameAs(other.state);}

        Character operator* () const;
        Character * operator-> () const;

        explicit operator AnyString() const;
    };

    friend class iterator;

    iterator begin();
    iterator end();

public:
    template <
        class T =
#if REN_CLASSLIB_STD
            std::string
#elif REN_CLASSLIB_QT
            QString
#else
    static_assert(false, "https://github.com/hostilefork/rencpp/issues/22");
#endif
    >
    T spellingOf() const {
        throw std::runtime_error("Unspecialized version of spellingOf called");
    }

#if REN_CLASSLIB_STD
    std::string spellingOf_STD() const;
#endif

#if REN_CLASSLIB_QT
    QString spellingOf_QT() const;
#endif

};


// http://stackoverflow.com/a/3052604/211160

#if REN_CLASSLIB_STD
template<>
inline std::string AnyString::spellingOf<std::string>() const {
    return spellingOf_STD();
}
#endif

#if REN_CLASSLIB_QT
template<>
inline QString AnyString::spellingOf<QString>() const {
    return spellingOf_QT();
}
#endif


class AnyBlock : public Series {
protected:
    friend class Value;
    AnyBlock (Dont const &) : Series (Dont::Initialize) {}
    inline bool isValid() const { return isAnyBlock(); }

protected:
    //
    // Provide a helper to the derived classes to construct AnyBlock
    // instances through the binding.  But you can't construct an "AnyBlock"
    // instance without a block type... it's abstract!  No such thing.
    //
    // For the curious: empty constructor has to result in an empty block.
    // No way to distinguish from the default constructor:
    //
    //     http://stackoverflow.com/a/9020606/211160
    //

    AnyBlock (
        RenCell loadables[],
        size_t numLoadables,
        bool (Value::*validMemFn)(RenCell *) const,
        Context * context = nullptr
    );
};



///
/// INTERNAL TEMPLATE HELPERS TO ASSIST CREATING CONCRETE VALUE CLASSES
///

//
// These are helper templates which are used to provide the base functions
// to their specific type classes.  They provide publicly inherited functions
// but are not meant to be directly instantiated by users.  They are
// templated by the member function which can both identify and set the
// class in a cell, defined by the runtime you are using.
//

namespace internal {


template <bool (Value::*validMemFn)(RenCell *) const>
class AnyWordSubtype : public AnyWord {
protected:
    friend class Value;
    AnyWordSubtype (Dont const &) : AnyWord (Dont::Initialize) {}
    inline bool isValid() const { return (this->*validMemFn)(nullptr); }

public:
    explicit AnyWordSubtype (
        char const * cstr,
        Context * context = nullptr
    ) :
        AnyWord (cstr, validMemFn, context)
    {
    }

#if REN_CLASSLIB_STD
    explicit AnyWordSubtype (
        std::string const & str,
        Context * context = nullptr
    ) :
        AnyWord (str.c_str(), validMemFn, context)
    {
    }
#endif

#if REN_CLASSLIB_QT
    explicit AnyWordSubtype (
        QString const & str,
        Context * context = nullptr
    ) :
        AnyWord (str, validMemFn, context)
    {
    }
#endif
};


template <bool (Value::*validMemFn)(RenCell *) const>
class AnyStringSubtype : public AnyString {
protected:
    friend class Value;
    AnyStringSubtype (Dont const &) : AnyString (Dont::Initialize) {}
    inline bool isValid() const { return (this->*validMemFn)(nullptr); }

public:
    explicit AnyStringSubtype (char const * cstr) :
        AnyString (cstr, validMemFn)
    {
    }

#if REN_CLASSLIB_STD
    explicit AnyStringSubtype (
        std::string const & str,
        Engine * engine = nullptr
    ) :
        AnyString (str.c_str(), validMemFn, engine)
    {
    }
#endif

#if REN_CLASSLIB_QT
    explicit AnyStringSubtype (
        QString const & str,
        Engine * engine = nullptr
    ) :
        AnyString (str, validMemFn, engine)
    {
    }
#endif
};


///
/// LAZY LOADING TYPE USED BY VARIADIC BLOCK CONSTRUCTORS
///

//
// Loadable is a "lazy-loading type" distinct from Value, which unlike a
// ren::Value can be implicitly constructed from a string and loaded as a
// series of values.  It's lazy so that it won't wind up being forced to
// interpret "foo baz bar" immediately as [foo baz bar], but to be able
// to decide if the programmer intent was to compose it together to form
// a single level of block hierarchy.
//
// See why Loadable doesn't let you say ren::Block {1, {2, 3}, 4} here:
//
//     https://github.com/hostilefork/rencpp/issues/1
//
// While private inheritance is one of those "frowned upon" institutions,
// here we really do want it.  It's a perfect fit for the problem.
//

class Loadable final : private Value {
private:
    friend class ::ren::Value;
    friend class ::ren::AnyWord;
    friend class ::ren::AnyString;
    friend class ::ren::Context;

    template <class C, bool (Value::*validMemFn)(RenCell *) const>
    friend class AnyBlockSubtype;

    // These constructors *must* be public, although we really don't want
    // users of the binding instantiating loadables explicitly.
public:
    using Value::Value;

    // Constructor inheritance does not inherit move or copy constructors

    Loadable (Value const & value) : Value (value) {}

    Loadable (Value && value) : Value (value) {}

    Loadable (char const * sourceCstr);
};


//
// AnyBlockSubtype
//
// This bears some explanation, because it's a little bit tricky.  There's a
// little bit of over-the-top avoidance of paying for the overhead
// of std::vector here and using std::array (just a compile-time prettifier
// on top of a C array):
//
//     http://stackoverflow.com/q/381621/211160
//
// But it's always good to dispel people's mythology that C++ is somehow
// "slower than C".  For a CodeReview.SE analysis of the technique used
// here to construct block types, see:
//
//     http://codereview.stackexchange.com/q/72252/9042
//
template <class C, bool (Value::*validMemFn)(RenCell *) const>
class AnyBlockSubtype : public AnyBlock {
protected:
    friend class Value;
    AnyBlockSubtype (Dont const &) : AnyBlock (Dont::Initialize) {}
    inline bool isValid() const { return (this->*validMemFn)(nullptr); }

public:
    AnyBlockSubtype (
        Value * values,
        size_t numValues,
        Context * context = nullptr
    ) :
        AnyBlock (
            numValues > 0 ? &values[0].cell : nullptr,
            numValues,
            validMemFn,
            context
        )
    {
    }

    explicit AnyBlockSubtype (
        std::initializer_list<internal::Loadable> args,
        Context * context = nullptr
    ) :
        AnyBlock (
            const_cast<RenCell *>(&args.begin()->cell),
            args.size(),
            validMemFn,
            context
        )
    {
    }

    explicit AnyBlockSubtype (Context * context = nullptr) :
        AnyBlock (
            nullptr,
            0,
            validMemFn,
            context
        )
    {
    }
};


} // end namespace internal



///
/// SPECIFIC CONCRETE VALUE CLASS PROXIES
///

//
// These classes represent concrete instances of a class, and are in the
// inheritance graph tracing back to their parent types (which may include
// a template helper).  Many could be typedefs instead of classes that
// inherit constructors.  But by making them their own named class the
// error messages while compiling them get clearer...and it becomes possible
// to add behaviors where needed.
//
// We do want to inherit the constructors of the subtype helpers though.
//


class Word final :
    public internal::AnyWordSubtype<&Value::isWord>
{
public:
    friend class Value;

    using AnyWordSubtype<&Value::isWord>::AnyWordSubtype;
};



class SetWord final :
    public internal::AnyWordSubtype<&Value::isSetWord>
{
public:
    friend class Value;
    using AnyWordSubtype<&Value::isSetWord>::AnyWordSubtype;
};



class GetWord final :
    public internal::AnyWordSubtype<&Value::isGetWord>
{
public:
    friend class Value;
    using AnyWordSubtype<&Value::isGetWord>::AnyWordSubtype;
};



class LitWord final :
    public internal::AnyWordSubtype<&Value::isLitWord>
{
public:
    friend class Value;
    using AnyWordSubtype<&Value::isLitWord>::AnyWordSubtype;
};



class Refinement final :
    public internal::AnyWordSubtype<&Value::isRefinement>
{
public:
    friend class Value;
    using AnyWordSubtype<&Value::isRefinement>::AnyWordSubtype;
};



class String final : public internal::AnyStringSubtype<&Value::isString>
{
public:
    friend class Value;
    using AnyStringSubtype<&Value::isString>::AnyStringSubtype;

public:
    // For String only, allow implicit cast instead of explicit.

#if REN_CLASSLIB_STD
    operator std::string () const {
        return Value::operator std::string ();
    }
#endif

#if REN_CLASSLIB_QT
    operator QString () const {
        return Value::operator QString ();
    }
#endif

    bool operator==(char const * cstr) const {
        return static_cast<std::string>(*this) == cstr;
    }

    bool operator!=(char const * cstr) const {
        return not (*this == cstr);
    }
};



class Tag final : public internal::AnyStringSubtype<&Value::isTag> {
public:
    friend class Value;

    using AnyStringSubtype<&Value::isTag>::AnyStringSubtype;

public:
    bool operator==(char const * cstr) const {
        return static_cast<std::string>(*this) == cstr;
    }

    bool operator!=(char const * cstr) const {
        return not (*this == cstr);
    }
};



class Block final :
    public internal::AnyBlockSubtype<Block, &Value::isBlock> {
public:
    friend class Value;
    using internal::AnyBlockSubtype<Block, &Value::isBlock>::AnyBlockSubtype;
};



class Paren final :
    public internal::AnyBlockSubtype<Paren, &Value::isParen> {
public:
    friend class Value;
    using internal::AnyBlockSubtype<Paren, &Value::isParen>::AnyBlockSubtype;
};



class Path final :
    public internal::AnyBlockSubtype<Path, &Value::isPath> {
public:
    friend class Value;
    using internal::AnyBlockSubtype<Path, &Value::isPath>::AnyBlockSubtype;
};


} // end namespace ren

#endif
