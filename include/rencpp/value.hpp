#ifndef RENCPP_VALUE_HPP
#define RENCPP_VALUE_HPP

//
// value.hpp
// This file is part of RenCpp
// Copyright (C) 2015-2018 HostileFork.com
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

#include "hooks.h"



namespace ren {


//
// FORWARD DEFINITIONS
//

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

class AnyValue;

class AnyContext;

class Engine;


namespace internal {
    //
    // Can't forward-declare in nested scopes, e.g. `class internal::Loadable`
    //
    //    http://stackoverflow.com/q/951234/211160
    //
    class Loadable;

    class AnySeries_;

    class RebolHooks;

    template <class R, class... Ts>
    class FunctionGenerator;

    // We want to be able to pass a Context to the constructors.  However, the
    // Context itself is a legal Ren type!  This "ContextWrapper" is used to
    // carry a context without itself being a candidate to be a Loadable.

    struct ContextWrapper {
        AnyContext const & context;
    };
}



//
// CAST EXCEPTION
//

//
// Although we can define most of the exceptions in exceptions.hpp, this one
// is thrown by AnyValue itself in templated code and needs to be here
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



// All ren::AnyValue types can be converted to a string, which under the hood
// invokes TO-STRING.  (It invokes the modified specification, which is
// an adaptation of what used to be called FORM).
//
// These functions take advantage of "Argument Dependent Lookup".  Though
// you can call them explicitly as e.g. ren::to_string(...), if you do
// `using std::to_string;` and then use the unqualified `to_string(...)`,
// it will notice that the argument is a ren:: type and pick these versions
//
// Note that there is no `to_string(optional<AnyValue>)`.  This is intentional,
// because it's more common to have special handling for a non-value case
// than not.  So silently compiling situations that could print "no value"
// (or whatever) undermines the benefit of compile-time safety C++ offers.

std::string to_string (AnyValue const & value);

#if REN_CLASSLIB_QT == 1
QString to_QString(AnyValue const & value);
#endif



//
// CELLFUNCTION
//

//
// The cellfunction is a particularly styled method on AnyValue, used by the
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

using CellFunction = void (*)(REBVAL *);

}



//
// VALUE BASE CLASS
//

//
// In the encapsulation as written, we pay additional costs for a reference
// count, plus a handle to which runtime instance the value belongs to.  While
// it may seem that adding another 64-bits is a lot to add to a cell...remember
// this is only for values that get bridged.  A series with a million elements
// in it is not suddenly costing 64-bits per element; the internals are
// managing the 128-bit cells and the series reference itself is the only
// one value that needs the overhead in the binding.
//
// !!! The new economics of Ren-C allows each AnyValue to become a REBSER,
// stored as a "singular array".  This will drastically change the behavior
// and is a pending change, but the short-term choice is to add a virtual
// destructor--as it is no longer a goal to have a cell footprint matching
// the cells of the language.
//

class AnyValue {
    //
    // The methods used by Ren-C's internal codebase to implement Rebol in C
    // are not easily miscible with generic C++ code.  As one simple example:
    // it defines global macros with names like fail(), and is hence
    // incompatible with any iostream code.
    //
    // Therefore the Ren-C internal headers (%sys-core.h) cannot be included
    // outside of carefully written code in the wrapper.  In time, RenCpp may
    // become based on the external C API (libRebol) as that fleshes out.
    // Until then, methods like these are used to expose common debug tools.
    //
#if !defined(NDEBUG)
public:
    void probe() const; // PROBE()
    void validate() const; // ASSERT_CONTEXT, etc.
#endif

    // Function needs access to the spec block's series cell in its creation.
    // Series_ wants to be able to just tweak the index of the cell
    // as it enumerates and leave the rest alone.  Etc.
    // There may be more "crossovers" of this form.  It's a tradeoff between
    // making cell public, using some kind of pimpl idiom or opaque type,
    // or making all AnyValue's derived classes friends of value.
protected:
    friend class ren::internal::Loadable; // for string class constructors
    friend class AnySeries; // !!! needs to write path cell in operator[] ?
    friend class Function; // needs to extract series from spec block
    friend class ren::internal::AnySeries_; // iterator state

    REBVAL *cell;

    friend class internal::RebolHooks;

    //
    // While "adding a few more bytes here and there" in Red and Rebol culture
    // is something that is considered a problem, this is a binding layer.  It
    // does not store a reference for each value in a series...if your series
    // is a million elements long and you hold a reference to that... you
    // aren't paying for the added storage of a AnyValue for each element.
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
    friend class AnyContext;
    RenEngineHandle origin;


    //
    // There is a default constructor, and it initializes the REBVAL to be
    // a constructed value of type BLANK!
    //
    // BUT as an implementation performance detail, if the default constructor
    // is bothering to initialize the 128 bits, what if a derived class
    // wants to do all its initialization in the constructor body?  Perhaps
    // the code uses code that would mean taking addresses of
    // temporaries and such, so it needs to construct the base class somehow.
    //
    //     DerivedType (...) : AnyValue(Dont::Initiailize)
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

protected:
    enum class Dont {Initialize};
    AnyValue (Dont);

    bool tryFinishInit(RenEngineHandle engine);

    inline void finishInit(RenEngineHandle engine) {
        if (!tryFinishInit(engine))
            throw std::runtime_error {"put meaningful error here"};
    }

    void uninitialize();

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

    explicit AnyValue (REBVAL *cell, RenEngineHandle engine) noexcept {
        this->cell = cell;
        finishInit(engine);
    }

    template<
        class T,
        typename = typename std::enable_if<
            std::is_base_of<AnyValue, T>::value
        >::type
    >
    static T fromCell_(
        REBVAL const * cell, RenEngineHandle engine
    ) noexcept {
        // Do NOT use {} construction!
        T result (Dont::Initialize);
        // If you use {} then if T is an array type, due to AnyValue's privileged
        // access to the Dont constructor, it will make a block with an
        // uninitialized value *in the block*!  :-/

        RL_Move(result.cell, cell);
        result.finishInit(engine);
        return result;
    }

    template<
        class T,
        typename = typename std::enable_if<
            std::is_base_of<AnyValue, utility::extract_optional_t<T>>::value
        >::type
    >
    static optional<utility::extract_optional_t<T>> fromCell_(
        REBVAL const * cell, RenEngineHandle engine
    ) noexcept {
        // Do NOT use {} construction!
        utility::extract_optional_t<T> result (Dont::Initialize);
        // If you use {} then if T is a series type, due to AnyValue's privileged
        // access to the Dont constructor, it will make a block with an
        // uninitialized value *in the block*!  :-/

        RL_Move(result.cell, cell);
        if (!result.tryFinishInit(engine))
            return nullopt;
        return result;
    }


public:
    static void toCell_(
        REBVAL *out, AnyValue const & value
    ) noexcept {
        RL_Move(out, value.cell);
    }

    static void toCell_(
        REBVAL *out, optional<AnyValue> const & value
    ) noexcept;

public:
    //
    // Though technically possible to just assign from the none class as
    // `ren::Blank{}`, it is slightly nicer to be able to use `ren::blank`.
    //
    //     https://github.com/hostilefork/rencpp/issues/3
    //
    // It is probably also more efficient (though this hasn't been verified)
    //

    AnyValue (std::nullptr_t) = delete; // vetoed!

    struct blank_t
    {
      struct init {};
      constexpr blank_t(init) {}
    };

    AnyValue (blank_t, Engine * engine = nullptr) noexcept;


    //
    // At first the only user-facing constructor that was exposed directly
    // from AnyValue was the default constructor.  It made a ren::Void
    // before that class was eliminated to embrace std::optional for the
    // purpose...now it makes a BLANK!:
    //
    //     ren::AnyValue something; // will be a BLANK! value
    //
    // But support for other construction types directly as AnyValue has been
    // incorporated.  For the rationale, see:
    //
    //     https://github.com/hostilefork/rencpp/issues/2
    //
public:
    // Default constructor; same as blank.

    AnyValue (Engine * engine = nullptr) noexcept :
        AnyValue(blank_t::init{}, engine)
    {}


public:
    //
    // We *explicitly* coerce to operator bool.  This only does the conversion
    // automatically under contexts like if(); the C++11 replacement for
    // "safe bool idiom" of if (!!x).
    //
    // http://stackoverflow.com/questions/6242768/
    //
    AnyValue (bool b, Engine * engine = nullptr) noexcept;

    bool isTruthy() const;

    bool isFalsey() const;

    // http://stackoverflow.com/q/6242768/211160
    explicit operator bool() const;

    //
    // The nasty behavior of implicitly converting pointers to booleans leads
    // to frustrating bugs... disable it!
    //
    //     https://github.com/hostilefork/rencpp/issues/24
    //

    template <typename T>
    AnyValue (const T *, Engine * = nullptr); // never define this!


public:
    AnyValue (char c, Engine * engine = nullptr);
    AnyValue (wchar_t wc, Engine * engine = nullptr) noexcept;

    AnyValue (int i, Engine * engine = nullptr) noexcept;

    // Literals are double by default unless you suffix with "f"
    //     http://stackoverflow.com/a/4353788/211160
    AnyValue (double d, Engine * engine = nullptr) noexcept;


public:
    // "Copy constructing" values--in the C++ sense--corresponds to assignment
    // in Rebol.  That's not "copying"...e.g. how `X: [1 2 3] | Y: X` does
    // not "copy".  It only copies the value *cell* itself.  So you get
    // an entity that can track a new position in something, while sharing the
    // identity of the payload.
    //
    // So each copy construct makes a new allocation of a pairing.  Thus if a
    // unique positioning is *not* needed, it's best to pass by `const &` (as
    // is true in C++ generally).
    //
    AnyValue (AnyValue const & other) noexcept :
        AnyValue (Dont::Initialize)
    {
        RL_Move(cell, other.cell);
        finishInit(other.origin);
    }

    // Move construction "takes over" the pairing.
    //
    // User-defined move constructors should not throw exceptions.  We
    // trust the C++ type system here.  You can move a String into an
    // AnySeries but not vice-versa.
    //
    AnyValue (AnyValue && other) noexcept :
        cell (other.cell)
    {
        if (other.cell) {
            other.cell = NULL;
            finishInit(other.origin);
        }
    }

    AnyValue & operator=(AnyValue const & other) noexcept {
        RL_Move(cell, other.cell);
        finishInit(other.origin); // increase new refcount
        return *this;
    }

public:
    virtual ~AnyValue () {
        if (cell) {
            uninitialize();
        #if !defined(NDEBUG)
            cell = NULL;
        #endif
        }
    }


    // Although C++ assignment and moving of values around is really just
    // effectively references, there is a copy method which corresponds to
    // the COPY command.
public:
    AnyValue copy(bool deep = true) const;


public:
    friend std::string to_string (AnyValue const & value);

#if REN_CLASSLIB_QT == 1
    friend QString to_QString(AnyValue const & value);
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
    // than overloading == and != (you cannot do both, it would cause
    // ambiguity...see issue).  But it's not a big loss anyway, because
    // C++ has a different meaning for == than Rebol/Red.
    //
    // Naming-wise, we map `equal?` to `isEqual`.  Then because it is
    // not prefix but "pseudo infix" due to the function call notation,
    // it reads more literately as `isEqualTo`.
    //
public:
    bool isEqualTo(AnyValue const & other) const;

    bool isSameAs(AnyValue const & other) const;


public:
    // Making AnyValue support -> is kind of wacky; it acts as a pointer to
    // itself.  But in the iterator model there aren't a lot of answers
    // for supporting the syntax.  It's true that conceptionally a series
    // does "point to" a value, so series->someValueMethod is interesting.
    //
    // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3723.html

    AnyValue const * operator->() const { return this; }
    AnyValue * operator->() { return this; }


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
    // The /ONLY option on APPLY is used to say whether to reduce the arguments
    // or not.  What's the right default?  We have the problem of there not
    // being anywhere to put a boolean in the call because it would be
    // assumed as a parameter...is a separate printOnly required?
    //
protected:
    optional<AnyValue> apply_(
        internal::Loadable const loadables[],
        size_t numLoadables,
        AnyContext const * contextPtr = nullptr,
        Engine * engine = nullptr
    ) const;

public:
    optional<AnyValue> apply(
        std::initializer_list<internal::Loadable> loadables,
        internal::ContextWrapper const & wrapper
    ) const;

    optional<AnyValue> apply(
        std::initializer_list<internal::Loadable> loadables,
        Engine * engine = nullptr
    ) const;

    template <typename... Ts>
    inline optional<AnyValue> apply(Ts const &... args) const {
        return apply({ args... });
    }

    // This is needed by the global free function `ren::hasTyPe()` to sneak
    // past non-friendedness status of it to derived-from-AnyValue classes to
    // use their static `isValid()` method.
protected:
    template <class T, class V> friend
    inline bool hasType(V const & value);

    template <class T, class V> friend
    inline bool hasType(optional<V> const & value);

    template <class T, class V>
    inline static bool hasTypeHelper(V const & value) {
        static_assert(
            std::is_base_of<AnyValue, V>::value,
            "Only types derived from AnyValue may be tested by ren::hasType()"
        );

        static_assert(
            std::is_base_of<V, T>::value,
            "Type test not possible for derived type in ren::hasType()"
        );

        return T::isValid(value.cell);
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
            std::is_base_of<AnyValue, T>::value
            && !std::is_same<AnyValue, T>::value
        >::type
    >
    explicit operator T () const
    {
        if (!T::isValid(cell))
            throw bad_value_cast("Invalid cast");

        // Here's the tough bit.  How do we throw exceptions on all the right
        // cases?  Each class needs a checker for the bits.  So it constructs
        // the instance, with the bits, but then throws if it's bad...and it's
        // not virtual.
        T result (Dont::Initialize);
        RL_Move(result.cell, cell);
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
        if (!T::isValid(cell))
            return false;

        T result (Dont::Initialize);
        RL_Move(result.cell, cell);
        result.finishInit(origin);

        return result.hasSpelling(spelling);
    }


protected:
    //
    // This hook might not be used by all value types to construct (and a
    // runtime may not be available to "apply" vs. merely construct).  Yet
    // because of its generality, it may be used by any implementation to
    // construct values... hence it makes sense to put it in AnyValue and
    // hence protectedly available to all subclasses.  It wraps the
    // "universal hook" in hooks.h to be safely used, with errors converted
    // to exceptions and finishInit() called on the "out" parameters
    //

    friend class Runtime;
    friend class Engine;

    static bool constructOrApplyInitialize(
        RenEngineHandle engine,
        AnyContext const * context,
        AnyValue const * applicand,
        internal::Loadable const loadables[],
        size_t numLoadables,
        AnyValue * constructOutTypeIn,
        AnyValue * applyOut
    );
};

inline std::ostream & operator<<(std::ostream & os, AnyValue const & value) {
    return os << to_string(value);
}

inline std::ostream & operator<<(
    std::ostream & os,
    optional<AnyValue> const & value
) {
    return value == nullopt ? os : os << to_string(*value);
}


// Initially the identifying functions for determining the concrete type
// of an AnyValue or subclass had names like `isInteger()` and `isBlock()`
// for "readability" at the callsites, and to avoid repeating namespacing
// e.g. `isType<ren::Integer>()` or `isType<ren::Block>()`.
//
// The up-in-the-air question of what IS will do in Ren-C led to changing the
// name to hasType, since IS may become a "fuzzy equality" in order to give
// back = to the concept of "real equality"
//
// Also, hasType was turned into a free function.  This means it can do
// static typing to make sure you don't try and do a test that isn't possible,
// such as testing a string to see if it's an integer.  Also, optionals can be
// tested by a variation and fail gracefully if the optional is disengaged.

template <class T, class V>
bool hasType(V const & value) {
    return AnyValue::hasTypeHelper<T, V>(value);
}

template <class T, class V>
bool hasType(optional<V> const & value) {
    if (value == nullopt)
        return false;
    return AnyValue::hasTypeHelper<T, V>(*value);
}


//
// NON-LOCAL-CONTROL REN-STYLE THROW
//

// In C++, the `throw` keyword is used almost entirely for error conditions.
// It is not a general-purpose mechanism for non-local control as it is in
// Rebol and Red.  The default behavior of a C++ `throw` of a Ren value is
// thus to enforce that value is an ERROR! and act like what it would call
// a `raise` operation.
//
// Hence if you want to mimic a Ren-style THROW in the way it defines the
// idea, you need to C++-throw an object to represent it.  Throwing something
// like a BREAK or CONTINUE from C++ code back into Rebol/Red is probably
// less common than wanting to raise an error, and you'd be needing an
// object if the Ren THROW were "named" anyway to carry the name value.

class evaluation_throw : public std::exception {
private:
    optional<AnyValue> thrownValue; // throw might not have a value, e.g. return
    optional<AnyValue> throwName;
    std::string whatString;

public:
    evaluation_throw (
        optional<AnyValue> const & value,
        optional<AnyValue> const & name = nullopt
    ) :
        thrownValue (value),
        throwName (name)
    {
        if (name == nullopt) {
            whatString = std::string("THROW:");
            whatString += " ";
            if (thrownValue == nullopt)
                whatString += "(no value)";
            else
                whatString += to_string(*thrownValue);
        }
        else {
            whatString = std::string("THROW/NAME:");
            whatString += " ";
            if (thrownValue == nullopt)
                whatString += "(no value)";
            else
                whatString += to_string(*thrownValue);
            whatString += " ";
            whatString += to_string(*throwName);
        }
    }

    char const * what() const noexcept override {
        return whatString.c_str();
    }

    optional<AnyValue> const & value() const noexcept {
        return thrownValue;
    }

    optional<AnyValue> const & name() const noexcept {
        return throwName;
    }
};


namespace internal {

//
// LAZY LOADING TYPE USED BY VARIADIC BLOCK CONSTRUCTORS
//

//
// Loadable is a "lazy-loading type" distinct from AnyValue, which unlike a
// ren::AnyValue can be implicitly constructed from a string and loaded as a
// series of values.  It's lazy so that it won't wind up being forced to
// interpret "foo baz bar" immediately as [foo baz bar], but to be able
// to decide if the programmer intent was to compose it together to form
// a single level of block hierarchy.
//
// There are subclasses of Loadable which are used to handle behavior of
// nested initializer lists, e.g. `ren::Block {1, {2, 3}, 4};`.  See the
// discussion and controversy about that feature here:
//
//     https://github.com/hostilefork/rencpp/issues/1
//
// Loadable does not publicly inherit from AnyValue, and is not currently
// intended to be a user-facing type (hence internal:: namespace).  They
// are implicitly constructed only.
//

class Loadable : public AnyValue {
private:
    friend class AnyValue;

    // These constructors *must* be public, although we really don't want
    // users of the binding instantiating loadables explicitly.
public:
    using AnyValue::AnyValue;

    // Constructor inheritance does not inherit move or copy constructors

    Loadable () = delete;

    Loadable (AnyValue const & value) :
        AnyValue (value)
    {
    }

    Loadable (AnyValue && value) :
        AnyValue (value)
    {
    }

    // !!! Review implications of when optional<AnyValue> is a specialization
    // that is the same size under the hood as AnyValue...could it be moved
    // efficiently?  For now encode unsetness in the runtime-specific info.
    Loadable (optional<AnyValue> const & value);

    Loadable (char const * source);

    template <typename T>
    Loadable (std::initializer_list<T> loadables) = delete;

    // After trying for a while with the assumption that a std::string or
    // a QString would be handled as source, the bias was shifted.  We
    // assume that if you were using a string *class*, then you default
    // to getting a ren::String value from it...while const char * continues
    // to be loaded as a run of source.

    Loadable (std::string const & source);

#if REN_CLASSLIB_QT == 1
    Loadable (QString const & source);
#endif
};

//
// This class is to be used instead of Loadable when one
// want to give semantics to untyped curly braces where
// Loadable is generally used. It does allow to write
// this kind of things:
//
// ren::Block {1, {2, 3}, 4};
//
// Block is configured so that it can be constructed from
// a list of BlockLoadable<Block>, which means that every
// untyped curly braces in a Block constructor will construct
// new instances of Block.
//

template <typename BracesT>
class BlockLoadable : public Loadable {
private:
    friend class AnyValue;

public:
    using Loadable::Loadable;

    BlockLoadable () :
        Loadable(BracesT{})
    {
    }

    //
    // If you get an error here such as "invalid use of
    // void expression", it means that you tried to use
    // a block type which does not support any implicit
    // construction from curly braces.
    //

    BlockLoadable (std::initializer_list<BlockLoadable<BracesT>> loadables) :
        Loadable(BracesT(loadables))
    {
    }
};

} // end namespace internal

} // end namespace ren

#endif
