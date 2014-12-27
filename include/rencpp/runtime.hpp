#ifndef RENCPP_RUNTIME_HPP
#define RENCPP_RUNTIME_HPP

#include <functional>

#include "common.hpp"
#include "values.hpp"
#include "exceptions.hpp"
#include "context.hpp"
#include "engine.hpp"

#include "printer.hpp"

namespace ren {

extern Printer print;

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
/// BASE RUNTIME CLASS
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
    Runtime ();

    template <typename... Ts>
    Value operator()(Ts... args) {
        return Context::runFinder(nullptr)(args...);
    }


    static bool needsRefcount(RenCell const & cell);

    static inline RenCell const & getCell(Value const & value) {
        return value.cell;
    }

    //
    // See discussion here about how many hook points there need to be into
    // the system, vs relying upon calling the evaluator with words:
    //
    //    https://github.com/hostilefork/rencpp/issues/8
    //
    static std::string form(Value const & value);

    virtual ~Runtime () {
    }
};


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
