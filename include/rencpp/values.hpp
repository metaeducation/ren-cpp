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

#include <cassert>
#include <initializer_list>
#include <iosfwd>
#include <stdexcept>
#include <utility> // std::forward

#include <atomic>
#include <type_traits>

#include <typeinfo> // std::bad_cast

#include "common.hpp"


extern "C" {
#include "hooks.h"
}


namespace ren {


///
/// FORWARD DEFINITIONS
///

//
// Abstractly speaking the runtime is a "separate thing" from the basic type
// list (which would be more akin to "Ren"):
//
//    https://github.com/humanistic/REN
//
// So a bare-bones interface could be made where the values themselves were
// "dead data"...and you always had to pass them to the appropriate
// runtime service.  While it might make the architectural lines a bit more
// rigid, it's lame because you couldn't do something like:
//
//     auto print = ren::Word("print");
//     print(10 + 20);
//
// So some of the runtime ideas like "Engine" and "Context" have leaked in
// for various parameterizations.  Yet they are only defined by pointers that
// default to null in the interface; a given RenCpp implementation could
// throw errors if they were ever non-null.
//


class Context;

class Engine;


namespace internal {
    //
    // Can't forward-declare in nested scopes, e.g. `class internal::Loadable`
    //
    //    http://stackoverflow.com/q/951234/211160
    //
    class Loadable;

    class Series_;

#ifndef REN_RUNTIME
#elif REN_RUNTIME == REN_RUNTIME_RED
    class FakeRedHooks;
#elif REN_RUNTIME == REN_RUNTIME_REBOL
    class RebolHooks;
#else
    static_assert(false, "Invalid runtime setting");
#endif

    template <class R, class... Ts>
    class FunctionGenerator;
}



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
    // Function needs access to the spec block's series cell in its creation.
    // Series_ wants to be able to just tweak the index of the cell
    // as it enumerates and leave the rest alone.  Etc.
    // There may be more "crossovers" of this form.  It's a tradeoff between
    // making cell public, using some kind of pimpl idiom or opaque type,
    // or making all Value's derived classes friends of value.
protected:
    friend class Function; // needs to extract series from spec block
    friend class ren::internal::Series_; // iterator state

    RenCell cell;

#ifndef REN_RUNTIME
#elif REN_RUNTIME == REN_RUNTIME_RED
    friend class FakeRedHooks;
#elif REN_RUNTIME == REN_RUNTIME_REBOL
    friend class internal::RebolHooks;
#else
    static_assert(false, "Invalid runtime setting");
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
    void finishInit(Engine * engine = nullptr);

    template<
        class T,
        typename = typename std::enable_if<
            std::is_base_of<Value, T>::value
        >::type
    >
    static T construct_(Dont const &) {
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

    explicit Value (RenCell const & cell, RenEngineHandle engine) {
        this->cell = cell;
        finishInit(engine);
    }

    explicit Value (RenCell const & cell, Engine * engine = nullptr) {
        this->cell = cell;
        finishInit(engine);
    }

    template<
        class T,
        typename = typename std::enable_if<
            std::is_base_of<Value, T>::value
        >::type
    >
    static T construct_(RenCell const & cell, RenEngineHandle engine) {
        T result {Dont::Initialize};
        result.cell = cell;
        result.finishInit(engine);
        return result;
    }


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
    struct unset_t
    {
      struct init {};
      constexpr unset_t(init) {}
    };

    Value (unset_t const &, Engine * engine = nullptr);

    bool isUnset() const;

    // Default constructor; same as unset.

    Value (Engine * engine = nullptr) : Value(unset_t::init{}, engine) {}


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

    // Value (nullptr_t) = delete; ...doesn't work in clang, bad idea?
    // Just make it a link error by never defining it
    Value (std::nullptr_t);

    struct none_t
    {
      struct init {};
      constexpr none_t(init) {}
    };

    Value (none_t const &, Engine * engine = nullptr);


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
    Value (bool const & b, Engine * engine = nullptr);

    bool isLogic() const;

    bool isTrue() const;

    bool isFalse() const;

    // http://stackoverflow.com/q/6242768/211160
    explicit operator bool() const;


public:
    Value (char const & c, Engine * engine = nullptr);
    Value (wchar_t const & wc, Engine * engine = nullptr);

    bool isCharacter() const;


public:
    Value (int const & i, Engine * engine = nullptr);

    bool isInteger() const;


public:
    // Literals are double by default unless you suffix with "f"
    //     http://stackoverflow.com/a/4353788/211160
    Value (double const & d, Engine * engine = nullptr);

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
                    "Refcounting problem reported by the Ren binding hook"
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
    // Making Value support -> is kind of wacky; it acts as a pointer to
    // itself.  But in the iterator model there aren't a lot of answers
    // for supporting the syntax.  It's true that conceptionally a series
    // does "point to" a value, so series->someValueMethod is interesting.
    //
    // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3723.html

    Value const * operator->() const { return this; }
    Value * operator->() { return this; }


public:

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
    Value apply_(
        internal::Loadable const loadables[],
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
        return apply({ args... });
    }

    template <typename... Ts>
    inline Value operator()(Ts&&... args) const {
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

protected:
    //
    // This hook might not be used by all value types to construct (and a
    // runtime may not be available to "apply" vs. merely construct).  Yet
    // because of its generality, it may be used by any implementation to
    // construct values... hence it makes sense to put it in Value and
    // hence protectedly available to all subclasses.  It wraps the
    // "universal hook" in hooks.h to be safely used, with errors converted
    // to exceptions and finishInit() called on the "out" parameters
    //

    friend class Runtime;
    friend class Engine;
    friend class Context;

    static void constructOrApplyInitialize(
        RenEngineHandle engine,
        RenContextHandle context,
        Value const * applicand,
        internal::Loadable const loadables[],
        size_t numLoadables,
        Value * constructOutTypeIn,
        Value * applyOut
    );
};

std::ostream & operator<<(std::ostream & os, Value const & value);



///
/// NONE AND UNSET CONSTRUCTION
///

//
// It makes sense to be able to create None and Unset *values*, which you
// can do with Value construction.  But why would you need a static type
// for the None and Unset *classes* in C++?  Do you need to statically
// check to make sure someone actually passed you a None?  :-/
//
// For completeness they are included for the moment, but are very unlikely
// to be useful in practice.  They really only make sense as instances of
// the Value base class.
//

constexpr Value::none_t none {Value::none_t::init{}};

constexpr Value::unset_t unset {Value::unset_t::init{}};

class Unset : public Value {
protected:
    friend class Value;
    Unset (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isUnset(); }

public:
    Unset (Engine * engine = nullptr) : Value (unset, engine) {}
};


class None : public Value {
protected:
    friend class Value;
    None (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isNone(); }

public:
    explicit None (Engine * engine = nullptr) : Value(none, engine) {}
};



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

class Logic : public Value {
protected:
    friend class Value;
    Logic (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isLogic(); }

public:
    // Narrow the construction?
    // https://github.com/hostilefork/rencpp/issues/24
    Logic (bool const & b, Engine * engine = nullptr) :
        Value (b, engine)
    {
    }

    operator bool () const;
};


class Character : public Value {
protected:
    friend class Value;
    friend class AnyString;
    Character (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isCharacter(); }

public:
    Character (char const & c, Engine * engine = nullptr) :
        Value (c, engine)
    {
    }

    Character (wchar_t const & wc, Engine * engine = nullptr) :
        Value (wc, engine)
    {
    }

    Character (int const & i, Engine * engine = nullptr);


    // Characters represent codepoints.  These conversion operators are for
    // convenience, but note that the char and wchar_t may throw if your
    // codepoint is too high for that; hence codepoint is explicitly long
    // (minimum range for int would be

    explicit operator char () const;
    explicit operator wchar_t () const;

    long codepoint() const;

    // How to expose UTF8 encoding?

#if REN_CLASSLIB_QT == 1
    operator QChar () const;
#endif

public:
    // Because Characters have a unique iteration in AnyString types, the
    // -> override mentioned in Value applies to them too.  See note there.

    Character const * operator->() const { return this; }
    Character * operator->() { return this; }
};


class Integer : public Value {
protected:
    friend class Value;
    Integer (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isInteger(); }

public:
    Integer (int const & i, Engine * engine = nullptr) :
        Value (i, engine)
    {
    }

    operator int () const;
};


class Float : public Value {
protected:
    friend class Value;
    Float (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isFloat(); }

public:
    Float (double const & d, Engine * engine = nullptr) :
        Value (d, engine)
    {
    }

    operator double () const;
};


class Date : public Value {
protected:
    friend class Value;
    Date (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isDate(); }

public:
    explicit Date (std::string const & str, Engine * engine = nullptr);
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
// It can not only test if a type is valid for its instance or category, but
// if you pass them a pointer to a cell they can write their type signature
// into that cell.  This helps avoid RTTI or virtual dispatch and in theory
// the reason templates can be parameterized with functions has to do with
// inlining and optimization...so it might be about the fastest way to do it.
//

namespace internal {

using CellFunction = bool (Value::*)(RenCell *) const;

// This class is necessary because we can't define a Series::iterator class
// to wrap a Series inside of a Series--it would be an incomplete definition

class Series_ : public Value {
protected:
    friend class Value;
    Series_ (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isSeries(); }

public:
    // We don't return values here because that would leak the internal
    // types.  It's technically possible to write a variant of things like
    // head() and tail() for every type but reinventing Rebol/Red is not
    // really the point.  Mutating types as they are vs. returning a new
    // base type could be a good option for working in the C++ world, if
    // iterators are available for things like enumerating.

    void operator++();
    void operator--();

    void operator++(int);
    void operator--(int);

    Value operator*() const;
    Value operator->() const; // see notes on Value::operator->

    void head();
    void tail();
};

} // end namespace internal


class AnyWord : public Value {
protected:
    friend class Value;
    AnyWord (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isAnyWord(); }

protected:
    explicit AnyWord (
        char const * cstr,
        internal::CellFunction cellfun,
        Context * context = nullptr
    );


#if REN_CLASSLIB_STD
    explicit AnyWord (
        std::string const & str,
        internal::CellFunction cellfun,
        Context * context = nullptr
    ) :
        AnyWord (str.c_str(), cellfun, context)
    {
    }
#endif


#if REN_CLASSLIB_QT
    explicit AnyWord (
        QString const & str,
        internal::CellFunction cellfun,
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



class Series : public ren::internal::Series_ {
protected:
    friend class Value;
    Series (Dont const &) : Series_ (Dont::Initialize) {}
    inline bool isValid() const { return isSeries(); }

    //
    // If you wonder why C++ would need a separate iterator type for a Series
    // instead of doing as Rebol does and just using a Series, see this:
    //
    //    https://github.com/hostilefork/rencpp/issues/25
    //
    // The series thus functions as the state, but is a separate type that
    // has to be wrapped up.
public:
    class iterator {
        friend class Series;
        internal::Series_ state;
        iterator (internal::Series_ const & state) :
            state (state)
        {
        }

    public:
        iterator & operator++() {
            ++state;
            return *this;
        }

        iterator & operator--() {
            --state;
            return *this;
        }

        iterator operator++(int) {
            auto temp = *this;
            operator++();
            return temp;
        }

        iterator operator--(int) {
            auto temp = *this;
            operator--();
            return temp;
        }

        bool operator==(iterator const & other)
            { return state.isSameAs(other.state); }
        bool operator!=(iterator const & other)
            { return not state.isSameAs(other.state); }

        Value operator * () const { return *state; }
        Value operator-> () const { return state.operator->(); }
    };

    iterator begin() const {
        return iterator (*this);
    }

    iterator end() const {
        auto temp = *this;
        temp.tail(); // see remarks on tail modifying vs. returning a value
        return iterator (temp);
    }
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
        internal::CellFunction cellfun,
        Engine * engine = nullptr
    );

#if REN_CLASSLIB_STD
    AnyString (
        std::string const & str,
        internal::CellFunction cellfun,
        Engine * engine = nullptr
    );
#endif

#if REN_CLASSLIB_QT
    AnyString (
        QString const & str,
        internal::CellFunction cellfun,
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
        Series state;
        iterator (Series const & state) :
            state (state)
        {
        }

    public:
        iterator & operator++() {
            ++state;
            return *this;
        }

        iterator & operator--() {
            --state;
            return *this;
        }

        iterator operator++(int) {
            auto temp = *this;
            operator++();
            return temp;
        }

        iterator operator--(int) {
            auto temp = *this;
            operator--();
            return temp;
        }

        bool operator==(iterator const & other)
            { return state.isSameAs(other.state); }
        bool operator!=(iterator const & other)
            { return not state.isSameAs(other.state); }

        Character operator * () const {
            return static_cast<Character>(*state);
        }
        Character operator-> () const {
            return static_cast<Character>(state.operator->()); }
    };

    iterator begin() const {
        return iterator (*this);
    }

    iterator end() const {
        auto temp = *this;
        temp.tail();
        return iterator (temp);
    }

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
        internal::Loadable const loadablesPtr[],
        size_t numLoadables,
        internal::CellFunction cellfun,
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


template <class C, CellFunction F>
class AnyWord_ : public AnyWord {
protected:
    friend class Value;
    AnyWord_ (Dont const &) : AnyWord (Dont::Initialize) {}
    inline bool isValid() const { return (this->*F)(nullptr); }

public:
    explicit AnyWord_ (char const * cstr, Context * context = nullptr) :
        AnyWord (cstr, F, context)
    {
    }

#if REN_CLASSLIB_STD
    explicit AnyWord_ (std::string const & str, Context * context = nullptr) :
        AnyWord (str.c_str(), F, context)
    {
    }
#endif

#if REN_CLASSLIB_QT
    explicit AnyWord_ (QString const & str, Context * context = nullptr) :
        AnyWord (str, F, context)
    {
    }
#endif
};


template <class C, CellFunction F>
class AnyString_ : public AnyString {
protected:
    friend class Value;
    AnyString_ (Dont const &) : AnyString (Dont::Initialize) {}
    inline bool isValid() const { return (this->*F)(nullptr); }

public:
    AnyString_ (char const * cstr, Engine * engine = nullptr) :
        AnyString (cstr, F, engine)
    {
    }

#if REN_CLASSLIB_STD
    AnyString_ (std::string const & str, Engine * engine = nullptr) :
        AnyString (str.c_str(), F, engine)
    {
    }
#endif

#if REN_CLASSLIB_QT
    AnyString_ (QString const & str, Engine * engine = nullptr) :
        AnyString (str, F, engine)
    {
    }
#endif

    bool isEqualTo(char const * cstr) const {
        return static_cast<std::string>(*this) == cstr;
    }
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

class Loadable : private Value {
private:
    friend class Value;

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
// AnyBlock Subtype Helper
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
template <class C, CellFunction F>
class AnyBlock_ : public AnyBlock {
protected:
    friend class Value;
    AnyBlock_ (Dont const &) : AnyBlock (Dont::Initialize) {}
    inline bool isValid() const { return (this->*F)(nullptr); }

public:
    AnyBlock_ (Value * values, size_t numValues, Context * context = nullptr) :
        AnyBlock (
            numValues > 0 ? &values[0].cell : nullptr,
            numValues,
            F,
            context
        )
    {
    }

    AnyBlock_ (
        std::initializer_list<Loadable> const & loadables,
        Context * context = nullptr
    ) :
        AnyBlock (loadables.begin(), loadables.size(), F, context)
    {
    }

    AnyBlock_ (Context * context = nullptr) :
        AnyBlock (nullptr, 0, F, context)
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


class Word : public internal::AnyWord_<Word, &Value::isWord>
{
public:
    friend class Value;
    using AnyWord_<Word, &Value::isWord>::AnyWord_;
};



class SetWord : public internal::AnyWord_<SetWord, &Value::isSetWord>
{
public:
    friend class Value;
    using AnyWord_<SetWord, &Value::isSetWord>::AnyWord_;
};



class GetWord : public internal::AnyWord_<GetWord, &Value::isGetWord>
{
public:
    friend class Value;
    using AnyWord_<GetWord, &Value::isGetWord>::AnyWord_;
};



class LitWord : public internal::AnyWord_<LitWord, &Value::isLitWord>
{
public:
    friend class Value;
    using AnyWord_<LitWord, &Value::isLitWord>::AnyWord_;
};



class Refinement : public internal::AnyWord_<Refinement, &Value::isRefinement>
{
public:
    friend class Value;
    using AnyWord_<Refinement, &Value::isRefinement>::AnyWord_;
};



class String : public internal::AnyString_<String, &Value::isString>
{
public:
    friend class Value;
    using AnyString_<String, &Value::isString>::AnyString_;

public:
    // For String only, allow implicit cast instead of explicit.

#if REN_CLASSLIB_STD
    operator std::string () const {
        return Value::operator std::string ();
    }
#endif

#if REN_CLASSLIB_QT
    operator QString () const;
#endif
};



class Tag : public internal::AnyString_<Tag, &Value::isTag> {
public:
    friend class Value;
    using AnyString_<Tag, &Value::isTag>::AnyString_;
};



class Block : public internal::AnyBlock_<Block, &Value::isBlock>
{
public:
    friend class Value;
    using internal::AnyBlock_<Block, &Value::isBlock>::AnyBlock_;
};



class Paren : public internal::AnyBlock_<Paren, &Value::isParen>
{
public:
    friend class Value;
    using internal::AnyBlock_<Paren, &Value::isParen>::AnyBlock_;
};



class Path : public internal::AnyBlock_<Path, &Value::isPath>
{
public:
    friend class Value;
    using internal::AnyBlock_<Path, &Value::isPath>::AnyBlock_;
};


} // end namespace ren

#endif
