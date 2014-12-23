#ifndef RENCPP_VALUES_HPP
#define RENCPP_VALUES_HPP


#include <string>
#include <iostream>
#include <cstdint>

#include <atomic>
#include <type_traits>
#include <array>

#include "common.hpp"

#include "exceptions.hpp"


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
}

template <class R, class... Ts> class Extension;

struct none_t;


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
    friend class Runtime;
    friend class Context; // Value::Dont::Initialize
public: // temporary for the lambda in function, find better way
    RenCell cell;

#ifndef LINKED_WITH_RED_AND_NOT_A_TEST
    friend class FakeRedHooks;
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
    typedef std::atomic<unsigned int> RefcountType;
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


protected:
    //
    // The value-from-cell constructor does not check the bits, and all cell
    // based constructors are not expected to either.  You trust they were
    // set up correctly OR if they are not, then the cast operator is tasked
    // with checking their invalidity and throwing an exception
    //
    template <class R, class... Ts> friend class Extension;
    explicit Value (Engine & engine, RenCell const & cell);


#ifndef DEBUG
public:
    void trackLifetime();
#endif

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

    bool isUnset() const;


protected:
    //
    // So we see there is a default constructor, and it initializes the 128
    // bit pattern to be an UNSET.
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
    // Call finishInit to set up the refcounting if necessary.
    //
    enum class Dont {Initialize};
    Value (Dont const &);

public: // temporary until the lambdas are friended in Extension
    void finishInit(RenEngineHandle engineHandle);


public:
    //
    // Constructing from nullptr is so ugly we disable it, but it's nice
    // to be able to just assign from "none".
    //
    // https://github.com/hostilefork/rencpp/issues/3
    //

    bool isNone() const;

    Value (nullptr_t) = delete;

    Value (Engine & engine, none_t const &);
    Value (none_t const &);


public:
    //
    // We *explicitly* coerce to operator bool.  This only does the conversion
    // automatically under contexts like if(); the C++11 replacement for
    // "safe bool idiom" of if (!!x)
    //
    // http://stackoverflow.com/questions/6242768/
    //
    Value (Engine & engine, bool const & b);
    Value (bool const & b);

    bool isLogic() const;

    bool isTrue() const;

    bool isFalse() const;

    inline explicit operator bool() const {
      // http://stackoverflow.com/q/6242768/211160
        return (not isNone()) and (not isFalse());
    }


public:
    Value (Engine & engine, int const & someInt);
    Value (int const & someInt);

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

public:
    bool isExtension() const;

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
    ~Value() {
        releaseRefIfNecessary();
    }

public:
    friend std::ostream & operator<<(std::ostream & os, Value const & value);

    // If you explicitly ask for it, all ren::Value types can be static_cast
    // to a std::string.  Internally it uses the runtime::form
    // which is (ideally) bound to the native form and cannot be changed.
    // Only the string type allows implicit casting, though it provides the
    // same answer.

    explicit operator std::string () const;


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
    // I'm calling it apply here for "generalized apply"; although it is
    // protected so people won't be using the term directly.  So far it's
    // the best generalization I have come up with which puts a nice syntax on
    // do (which is a C++ keyword and not available)
    //
protected:
    Value apply(
        Context * contextPtr,
        internal::Loadable loadables[],
        size_t numLoadables
    );

public:
    template <typename... Ts>
    inline Value operator()(Context & context, Ts... args) {
        // http://stackoverflow.com/q/14178264/211160
        auto loadables = std::array<
            internal::Loadable, sizeof...(Ts)
        >{{args...}};
        return apply(&context, &loadables[0], sizeof...(Ts));
    }

    template <typename... Ts>
    inline Value operator()(Ts... args) {
        // http://stackoverflow.com/q/14178264/211160
        auto loadables = std::array<
            internal::Loadable, sizeof...(Ts)
        >{{args...}};
        return apply(nullptr, &loadables[0], sizeof...(Ts));
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
            && !std::is_same<Value, T>::value,
            T
        >::type
    >
    explicit operator T ()
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
//    Foo (Engine & engine, RenCell const & cell) : Value(engine, cell) {}
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
    template <class R, class... Ts> friend class Extension;
    Unset (Engine & engine, RenCell const & cell) : Value(engine, cell) {}
    Unset (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isUnset(); }

public:
    Unset () :
        Value ()
    {
    }
};


class None final : public Value {
protected:
    friend class Value;
    template <class R, class... Ts> friend class Extension;
    None (Engine & engine, RenCell const & cell) : Value(engine, cell) {}
    None (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isUnset(); }

public:
    explicit None (Engine & engine);
    explicit None ();
};


class Logic final : public Value {
protected:
    friend class Value;
    template <class R, class... Ts> friend class Extension;
    Logic (Engine & engine, RenCell const & cell) : Value(engine, cell) {}
    Logic (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isLogic(); }

public:
    Logic (bool const & b) :
        Value (b)
    {
    }

    operator bool () const;
};


class Integer final : public Value {
protected:
    friend class Value;
    template <class R, class... Ts> friend class Extension;
    Integer (Engine & engine, RenCell const & cell) : Value(engine, cell) {}
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
    template <class R, class... Ts> friend class Extension;
    Float (Engine & engine, RenCell const & cell) : Value(engine, cell) {}
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
    template <class R, class... Ts> friend class Extension;
    Date (Engine & engine, RenCell const & cell) : Value(engine, cell) {}
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
    template <class R, class... Ts> friend class Extension;
    AnyWord (Dont const &) : Value (Dont::Initialize) {}
    AnyWord (Engine & engine, RenCell const & cell) : Value(engine, cell) {}
    inline bool isValid() const { return isAnyWord(); }

protected:
    explicit AnyWord (
        Context & context, char const * cstr,
        bool (Value::*validMemFn)(RenCell *) const
    );

    explicit AnyWord (
        char const * cstr,
        bool (Value::*validMemFn)(RenCell *) const);


    explicit AnyWord (
        Context & context,
        std::string const & str,
        bool (Value::*validMemFn)(RenCell *) const
    ) :
        AnyWord (context, str.c_str(), validMemFn)
    {
    }

    explicit AnyWord (
        std::string const & str,
        bool (Value::*validMemFn)(RenCell *) const
    ) :
        AnyWord (str.c_str(), validMemFn)
    {
    }
};


class Series : public Value {
protected:
    friend class Value;
    template <class R, class... Ts> friend class Extension;
    Series (Engine & engine, RenCell const & cell) : Value(engine, cell) {}
    Series (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return isSeries(); }
};



class AnyString : public Series
{
protected:
    friend class Value;
    template <class R, class... Ts> friend class Extension;
    AnyString (Engine & engine, RenCell const & cell) : Series(engine, cell) {}
    AnyString (Dont const &) : Series (Dont::Initialize) {}
    inline bool isValid() const { return isAnyString(); }

protected:
    AnyString(
        Context & context,
        char const * cstr,
        bool (Value::*validMemFn)(RenCell *) const
    );

    AnyString(
        char const * cstr,
        bool (Value::*validMemFn)(RenCell *) const
    );

public:
    // This lets you pass ren::String to anything that expected a std::string
    // and do so implicitly, which may or may not be a great idea.  See:
    //
    //     https://github.com/hostilefork/rencpp/issues/6

    operator std::string () const {
        Value const & thisValue = *this;
        return static_cast<std::string>(thisValue);
    }

public:
    // Need to expose an iteration interface.  You should be able to write:
    //
    //     ren::String test {"Test"};
    //     for (auto ch = test) {
    //         ...
    //     }
    //
    // But what type do you get from the auto?
};



class AnyBlock : public Series {
protected:
    friend class Value;
    template <class R, class... Ts> friend class Extension;
    AnyBlock (Engine & engine, RenCell const & cell) : Series(engine, cell) {}
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
        Context & context,
        internal::Loadable * loadablesPtr,
        size_t numLoadables,
        bool (Value::*validMemFn)(RenCell *) const
    );

    AnyBlock (
        internal::Loadable * loadablesPtr,
        size_t numLoadables,
        bool (Value::*validMemFn)(RenCell *) const
    );

public:
    // Need to expose an iteration interface.  An AnyBlock iterator will always
    // give back Value, so we can get typing from that.
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
    template <class R, class... Ts> friend class Extension;
    AnyWordSubtype (Engine & engine, RenCell const & cell) : AnyWord(engine, cell) {}
    AnyWordSubtype (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return (this->*validMemFn)(nullptr); }

public:
    explicit AnyWordSubtype (Context & context, char const * cstr) :
        AnyWord (context, cstr, validMemFn)
    {
    }
    explicit AnyWordSubtype (char const * cstr) :
        AnyWord (cstr, validMemFn)
    {
    }

    explicit AnyWordSubtype (Context & context, std::string const & str) :
        AnyWord (context, str.c_str(), validMemFn)
    {
    }
    explicit AnyWordSubtype (std::string const & str) :
        AnyWord (str.c_str(), validMemFn)
    {
    }
};


template <bool (Value::*validMemFn)(RenCell *) const>
class AnyStringSubtype : public AnyString {
protected:
    friend class Value;
    template <class R, class... Ts> friend class Extension;
    AnyStringSubtype (Engine & engine, RenCell const & cell) : AnyString(engine, cell) {}
    AnyStringSubtype (Dont const &) : AnyString (Dont::Initialize) {}
    inline bool isValid() const { return (this->*validMemFn)(nullptr); }

public:
    explicit AnyStringSubtype (char const * cstr) :
        AnyString (cstr, validMemFn)
    {
    }

    explicit AnyStringSubtype (std::string const & str) :
        AnyString (str.c_str(), validMemFn)
    {
    }
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
template <bool (Value::*validMemFn)(RenCell *) const>
class AnyBlockSubtype : public AnyBlock {
protected:
    friend class Value;
    template <class R, class... Ts> friend class Extension;
    AnyBlockSubtype (Engine & engine, RenCell const & cell) : AnyBlock(engine, cell) {}
    AnyBlockSubtype (Dont const &) : Value (Dont::Initialize) {}
    inline bool isValid() const { return (this->*validMemFn)(nullptr); }

private:
    template <size_t N>
    AnyBlockSubtype (
        Context & context,
        std::array<internal::Loadable, N> && loadables
    ) :
        AnyBlock (context, loadables.data(), N, validMemFn)
    {
    }

    template <size_t N>
    AnyBlockSubtype (
        std::array<internal::Loadable, N> && loadables
    ) :
        AnyBlock (loadables.data(), N, validMemFn)
    {
    }

public:
    template <typename... Ts>
    explicit AnyBlockSubtype (Context & context, Ts const & ...args) :
        AnyBlockSubtype (
            context,
            std::array<internal::Loadable, sizeof...(args)>{{args...}}
        )
    {
    }

    template <typename... Ts>
    explicit AnyBlockSubtype (Ts const & ...args) :
        AnyBlockSubtype (
            std::array<internal::Loadable, sizeof...(args)>{{args...}}
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
    public internal::AnyWordSubtype<&Value::isWord> {
public:
    friend class Value;
    template <class R, class... Ts> friend class Extension;
    using AnyWordSubtype<&Value::isWord>::AnyWordSubtype;
};



class SetWord final :
    public internal::AnyWordSubtype<&Value::isSetWord> {
public:
    friend class Value;
    template <class R, class... Ts> friend class Extension;
    using AnyWordSubtype<&Value::isSetWord>::AnyWordSubtype;
};



class GetWord final :
    public internal::AnyWordSubtype<&Value::isGetWord> {
public:
    friend class Value;
    template <class R, class... Ts> friend class Extension;
    using AnyWordSubtype<&Value::isGetWord>::AnyWordSubtype;
};



class LitWord final :
    public internal::AnyWordSubtype<&Value::isLitWord> {
public:
    friend class Value;
    template <class R, class... Ts> friend class Extension;
    using AnyWordSubtype<&Value::isLitWord>::AnyWordSubtype;
};



class Refinement final :
    public internal::AnyWordSubtype<&Value::isRefinement> {
public:
    friend class Value;
    template <class R, class... Ts> friend class Extension;
    using AnyWordSubtype<&Value::isRefinement>::AnyWordSubtype;
};



class String final : public internal::AnyStringSubtype<&Value::isString> {
public:
    friend class Value;
    template <class R, class... Ts> friend class Extension;
    using AnyStringSubtype<&Value::isString>::AnyStringSubtype;

public:
    // For String only, allow implicit cast instead of explicit.

    operator std::string () const {
        return Value::operator std::string ();
    }

    bool operator==(char const * cstr) const {
        return static_cast<std::string>(*this) == cstr;
    }

    bool operator!=(char const * cstr) const {
        return not (*this == cstr);
    }
};




class Block final : public internal::AnyBlockSubtype<&Value::isBlock> {
public:
    friend class Value;
    template <class R, class... Ts> friend class Extension;
    using AnyBlockSubtype<&Value::isBlock>::AnyBlockSubtype;
};



class Paren final : public internal::AnyBlockSubtype<&Value::isParen> {
public:
    friend class Value;
    template <class R, class... Ts> friend class Extension;
    using AnyBlockSubtype<&Value::isParen>::AnyBlockSubtype;
};



class Path final : public internal::AnyBlockSubtype<&Value::isPath> {
public:
    friend class Value;
    template <class R, class... Ts> friend class Extension;
    using AnyBlockSubtype<&Value::isPath>::AnyBlockSubtype;
};


} // end namespace ren

#endif
