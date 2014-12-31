#ifndef RENCPP_RUNTIME_HPP
#define RENCPP_RUNTIME_HPP

#include <functional>

#include <stdexcept>
#include <iostream>
#include <iomanip>

#include "common.hpp"
#include "values.hpp"
#include "exceptions.hpp"
#include "context.hpp"
#include "engine.hpp"


namespace ren {

class Runtime;


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

namespace internal {

class Loadable final : private Value {
private:
    friend class ::ren::Context;

    // These constructors *must* be public, although we really don't want
    // users of the binding instantiating loadables explicitly.
public:
    using Value::Value;

    // Constructor inheritance does not inherit move or copy constructors

    Loadable (Value const & value) : Value (value) {}

    Loadable (Value && value) : Value (value) {}

    Loadable (char const * sourceCstr);
};

}



///
/// ABSTRACT BASE RUNTIME CLASS
///

//
// Since ren::runtime is just an instance of a Runtime, you can write:
//
//     ren::runtime("print", "{hello}");
//
// That will find the appropriate runtime using the EngineFinder which
// is in effect.  It might be a little deceptive, giving the illusion of a
// "single environment".  But the goal is to make it convenient for the most
// common case, while providing more control if needed.
//


class Runtime {
protected:
    // Generalized apply for runtime; if valuePtr is nullptr then it will
    // effectively DO the loadables.  The loadables are mutable and you are
    // free to use their loaded state
    friend class AnyBlock;
    friend class Value;
    friend class AnyString;
    friend class AnyWord;
    friend class Engine;


public:
    template <typename... Ts>
    Value operator()(Ts... args) {
        return Context::runFinder(nullptr)(args...);
    }


    virtual std::ostream & setOutputStream(std::ostream & os) = 0;

    virtual std::istream & setInputStream(std::istream & is) = 0;

    virtual std::ostream & getOutputStream() = 0;

    virtual std::istream & getInputStream() = 0;

    //
    // How to do a cancellation interface properly in threading environments
    // which may be making many requests?  This simple interface assumes one
    // evaluator thread to whom a cancel is being made from another thread
    // (that is not able to do evaluations at the same time)... because that
    // is what Rebol implemented.  A better interface is needed.
    //
    //     https://github.com/hostilefork/rencpp/issues/19
    //
    virtual void cancel() = 0;

    static bool needsRefcount(RenCell const & cell);

    static inline RenCell const & getCell(Value const & value) {
        return value.cell;
    }

    virtual ~Runtime () {
    }
};



///
/// PRINTING HELPER CLASS EXPERIMENT
///

//
// One misses Rebol/Red PRINT when doing debugging, so this little printer
// class brings you that functionality via variadic functions.  It's easy
// to use, just say:
//
//     ren::print("This", "will", "have", "spaces");
//
// To not get the spaces, use the .only function call.
//
//     ren::print.only("This", "won't", "be", "spaced");
//
// This is JUST AN EXPERIMENT to see how people might use it if it were
// available.  By writing it this way and not calling into the evaluator
// it will not match up with what print does necessarily, even if it were
// a complete reimplementation of the default print behavior (it is not).
//


class Printer {
private:
    Runtime & runtime;

public:
    Printer (Runtime & runtime)
        : runtime (runtime)
    {
    }

    template <typename T>
    void writeArgs(bool spaced, T && t) {
        UNUSED(spaced);
        runtime.getOutputStream() << std::forward<T>(t);
    }

    template <typename T, typename... Ts>
    void writeArgs(bool spaced, T && t, Ts &&... args) {
        writeArgs(spaced, std::forward<T>(t));
        if (spaced)
            runtime.getOutputStream() << " ";
        writeArgs(spaced, std::forward<Ts>(args)...);
    }

    template <typename... Ts>
    void corePrint(bool spaced, bool linefeed, Ts &&... args) {
        writeArgs(spaced, std::forward<Ts>(args)...);
        if (linefeed)
            runtime.getOutputStream() << std::endl;
    }

    template <typename... Ts>
    void operator()(Ts &&... args) {
        corePrint(true, true, std::forward<Ts>(args)...);
    }

    template <typename... Ts>
    void only(Ts &&... args) {
        corePrint(false, false, std::forward<Ts>(args)...);
    }

    ~Printer () {
    }
};

extern Printer print;


}


#ifndef REN_RUNTIME

static_assert(false, "No runtime defined");

#elif REN_RUNTIME == REN_RUNTIME_RED

#include "rencpp/red.hpp"

#elif REN_RUNTIME == REN_RUNTIME_REBOL

#include "rencpp/rebol.hpp"

#else

static_assert(false, "No runtime defined");

#endif

#endif
