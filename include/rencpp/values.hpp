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

class Value;

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



// All ren::Value types can be converted to a string, which under the hood
// invokes TO-STRING.  (It invokes the modified specification, which is
// an adaptation of what used to be called FORM).
//
// These functions take advantage of "Argument Dependent Lookup".  Though
// you can call them explicitly as e.g. ren::to_string(...), if you do
// `using std::to_string;` and then use the unqualified `to_string(...)`,
// it will notice that the argument is a ren:: type and pick these versions

#if REN_CLASSLIB_STD == 1
std::string to_string (Value const & value);
#endif

#if REN_CLASSLIB_QT == 1
QString to_QString(Value const & value);
#endif



///
/// CELLFUNCTION
///

//
// The cellfunction is a particularly styled method on Value, used by the
// "subtype helpers" to identify types.  This pointer takes the place of having
// to expose an enumerated type in the interface for the cell itself.
//
// It can not only test if a type is valid for its instance or category, but
// if you pass them a pointer to a cell they can write their type signature
// into that cell.  This helps avoid RTTI or virtual dispatch and in theory
// the reason templates can be parameterized with functions has to do with
// inlining and optimization...so it might be about the fastest way to do it.
//
// The helper templates themselves provide the base functions to their
// specific type classes.  They provide publicly inherited functions
// but are not meant to be directly instantiated by users.  They are
// templated by the member function which can both identify and set the
// class in a cell, defined by the runtime you are using.
//


namespace internal {

using CellFunction = bool (Value::*)(RenCell *) const;

}



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
    // While "adding a few more bytes here and there" in Red and Rebol culture
    // is something that is considered a problem, this is a binding layer.  It
    // does not store a reference for each value in a series...if your series
    // is a million elements long and you hold a reference to that... you
    // aren't paying for the added storage of a Value for each element.
    //
    // Perhaps a raw cell based interface has applications, but really, isn't
    // the right "raw cell based interface" just to program in Red/System?
    //
    // In any case, the API is set up in such a way that we could have values
    // coming from different engines, and there is no enforcement that the
    // internals remember which handles go with which engine.  This little
    // bit of per-value bookkeeping helps keep things straight when we
    // release the value for GC.
    //

protected:
    friend class Context;
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
    // It's not a common idiom, but precedent exists e.g. Qt::Uninitialized:
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
    Value (Dont);

    void finishInit(RenEngineHandle engine);

    template<
        class T,
        typename = typename std::enable_if<
            std::is_base_of<Value, T>::value
        >::type
    >
    static T construct_(Dont) {
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

    Value (unset_t, Engine * engine = nullptr);

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

    Value (std::nullptr_t) = delete; // vetoed!

    struct none_t
    {
      struct init {};
      constexpr none_t(init) {}
    };

    Value (none_t, Engine * engine = nullptr);


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
    Value (bool b, Engine * engine = nullptr);

    bool isLogic() const;

    bool isTrue() const;

    bool isFalse() const;

    // http://stackoverflow.com/q/6242768/211160
    explicit operator bool() const;

    //
    // The nasty behavior of implicitly converting pointers to booleans leads
    // to frustrating bugs... disable it!
    //
    //     https://github.com/hostilefork/rencpp/issues/24
    //

    template <typename T>
    Value (const T *, Engine * = nullptr); // never define this!


public:
    Value (char c, Engine * engine = nullptr);
    Value (wchar_t wc, Engine * engine = nullptr);

    bool isCharacter() const;


public:
    Value (int i, Engine * engine = nullptr);

    bool isInteger() const;


public:
    // Literals are double by default unless you suffix with "f"
    //     http://stackoverflow.com/a/4353788/211160
    Value (double d, Engine * engine = nullptr);

    bool isFloat() const;


public:
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

    bool isFilename(RenCell * = nullptr) const;

public:
    bool isFunction() const;

    bool isContext(RenCell * = nullptr) const;

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


public:
#if REN_CLASSLIB_STD == 1
    friend std::string to_string (Value const & value);
#endif

#if REN_CLASSLIB_QT == 1
    friend QString to_QString(Value const & value);
#endif


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
    // This is also known as "generalized apply", which in Ren Garden actually
    // is defined to be the meaning of APPLY.  It will hopefully be adopted
    // by both Rebol and Red's core implementations.
    //
protected:
    Value apply_(
        internal::Loadable const loadables[],
        size_t numLoadables,
        Context const * contextPtr = nullptr,
        Engine * engine = nullptr
    ) const;

public:
    Value apply(
        std::initializer_list<internal::Loadable> loadables,
        Context const & context
    ) const;

    Value apply(
        std::initializer_list<internal::Loadable> loadables,
        Engine * engine = nullptr
    ) const;

    template <typename... Ts>
    inline Value apply(Ts const &... args) const {
        return apply({ args... });
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

        if (not result.isValid())
            throw bad_value_cast("Invalid cast");

        // All constructed types, even Dont::Initialize, must be able to
        // survive a throw.

        result.finishInit(origin);
        return result;
    }


public:
    // This can probably be done more efficiently, but the idea of wanting
    // to specify a type and a spelling in a single check without having
    // to go through a cast is a nice convenient.  Only works for types
    // that have a "hasSpelling" method (strings, words)

    template <class T>
    bool isEqualTo(char const * spelling) const {
        T result (Dont::Initialize);
        result.cell = cell;

        if (not result.isValid())
            return false;

        // No need to finishInit() initialization.

        return result.hasSpelling(spelling);
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

    static void constructOrApplyInitialize(
        RenEngineHandle engine,
        Context const * context,
        Value const * applicand,
        internal::Loadable const loadables[],
        size_t numLoadables,
        Value * constructOutTypeIn,
        Value * applyOut
    );
};

inline std::ostream & operator<<(std::ostream & os, Value const & value) {
    return os << to_string(value);
}



namespace internal {

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

    Loadable ();

    Loadable (Value const & value) : Value (value) {}

    Loadable (Value && value) : Value (value) {}

    Loadable (char const * source);

    Loadable (std::initializer_list<Loadable> args);

#if REN_CLASSLIB_STD == 1
    Loadable (std::string const & source) :
        Loadable (source.c_str())
    {
    }
#endif

#if REN_CLASSLIB_QT == 1
    // TBD
#endif
};

} // end namespace internal

} // end namespace ren

#endif
