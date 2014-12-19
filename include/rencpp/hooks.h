#ifndef RENCPP_HOOKS_H
#define RENCPP_HOOKS_H

/*
// For the sake of the thought experiment, this file a generalized one that
// can be built with either C or C++.  One might consider how much work could
// potentially be reused between the two bindings.
//
// Terminology-wise, the behavior on the Red side is called the "Hook".  While
// behavior on the C++ side is called the "Binding".
*/



/*
// int32_t is in #include <stdint.h>, generally you should only use the
// specific size definitions when doing external interfaces like this.
//
// http://stackoverflow.com/q/6144682/211160
*/
#include <stdint.h>


/*
// Portable compile-time assert for C
// http://stackoverflow.com/a/809465/211160
*/
#define CASSERT(predicate, file) _impl_CASSERT_LINE(predicate,__LINE__,file)
#define _impl_PASTE(a,b) a##b
#define _impl_CASSERT_LINE(predicate, line, file) \
    typedef char _impl_PASTE(assertion_failed_##file##_,line)[2*!!(predicate)-1];


#define REN_RUNTIME_RED 304
#define REN_RUNTIME_REBOL 1020


#ifndef REN_RUNTIME

/*
// Need to define REN_RUNTIME, ensure you built CMake with either:
// -DRUNTIME=rebol or -DRUNTIME=red
*/
CASSERT(0, hooks_h)

#elif REN_RUNTIME == REN_RUNTIME_RED
/*
// RedCell
//
// Sometimes this fixed-size cell holds a value in its entirety--such as
// an integer! or a date!.  For variable sized types such as series, the
// values are only references to the data.  The cells thus contain a pointer
// to the NODE! where the actual data is stored.  Also in the cell is an
// index of where the reference is offset into the series.
//
// If a value contains a NODE! as a reference to the *actual* data, then that
// is an abstraction that is good for the lifetime of the series.  Modulo
// garbage collection, the 128-bit sequence is guaranteed to represent the
// same series for all time.  The content of the series may change, the
// embedded index may "go stale", but will not crash.  Look at:
//
//   https://github.com/red/red/blob/master/runtime/allocator.reds
//
// It tells us the bits are laid out like this:
//
// ;-- cell header bits layout --
// ;   31:     lock           ;-- lock series for active thread access only
// ;   30:     new-line       ;-- new-line (LF) marker (before the slot)
// ;   29-25:  arity          ;-- arity for routine! functions.
// ;   24:     self?          ;-- self-aware context flag
// ;   23:     node-body      ;-- op! body points to a block node
// ;                          ;--     (instead of native code)
// ;   22-8:   <reserved>
// ;   7-0:    datatype ID    ;-- datatype number
//
// cell!: alias struct! [
//    header  [integer!]      ;-- cell's header flags
//    data1   [integer!]      ;-- placeholders to make a 128-bit cell
//    data2   [integer!]
//    data3   [integer!]
// ]
//
// More specific interpretations of the 128 bits are given in the file:
//
//   https://github.com/red/red/blob/master/runtime/datatypes/structures.reds
//
*/

struct RedCell {
    int32_t header;
    int32_t data1;

    //
    // C and C++ compilers actually have something called the "strict aliasing
    // requirement".  There are a lot of common mistakes with it:
    //
    //     http://stackoverflow.com/questions/98650/
    //
    // Long story short: if a type was of one kind, you cannot reliably cast
    // it to another data type and read or write from it (unless the type you
    // cast to was char*, it's an exception).  When optimizing and being
    // concerned about whether to re-fetch a variable or use the last value,
    // it is driven by knowledge of the type system; every write to every
    // type does not cause it to assume anything in the world could have
    // changed.  For this reason we can't just cast one struct to another.
    //

    //
    // For union initializer list syntax, see here:
    //     http://stackoverflow.com/questions/18411039/
    //
    union {
        double dataD;
        struct {
            int32_t data2;
            int32_t data3;
        } s;
    };
};

typedef int32_t RedEngineHandle;
#define RED_ENGINE_HANDLE_INVALID 0

typedef int32_t RedContextHandle;
#define RED_CONTEXT_HANDLE_INVALID 0

typedef RedCell RenCell;

typedef RedEngineHandle RenEngineHandle;
#define REN_ENGINE_HANDLE_INVALID RED_ENGINE_HANDLE_INVALID

typedef RedContextHandle RenContextHandle;
#define REN_CONTEXT_HANDLE_INVALID RED_ENGINE_HANDLE_INVALID

#elif REN_RUNTIME == REN_RUNTIME_REBOL

// While Red has no C headers for us to include (being written in Red and
// Red/System), Rebol does.

#include "rebol/src/include/sys-core.h"
#include "rebol/src/include/reb-ext.h"
#include "rebol/src/include/reb-lib.h"


//
// REBVAL is a typedef for Reb_Value, both defined in src/include/sys_value.h
//
// The Rebol codebase is (circa 2014) not able to build with a C++ compiler,
// although patches do exist to make it possible.  Which is okay because you
// can link the C and C++ .o files together from the separate builds.  Here
// we are only sharing header files and macros, and linking through extern "C"
//

typedef REBVAL RenCell;

struct RebolEngineHandle {
    int data;
};
const struct RebolEngineHandle REBOL_ENGINE_HANDLE_INVALID = {-1};
#define REBOL_IS_ENGINE_HANDLE_INVALID(handle) \
    ((handle).data == REBOL_ENGINE_HANDLE_INVALID.data)

typedef RebolEngineHandle RenEngineHandle;
#define REN_ENGINE_HANDLE_INVALID REBOL_ENGINE_HANDLE_INVALID
#define REN_IS_ENGINE_HANDLE_INVALID REBOL_IS_ENGINE_HANDLE_INVALID

struct RebolContextHandle {
    REBSER * series;
};
const struct RebolContextHandle REBOL_CONTEXT_HANDLE_INVALID = {nullptr};
#define REBOL_IS_CONTEXT_HANDLE_INVALID(handle) \
    ((handle).series == REBOL_CONTEXT_HANDLE_INVALID.series)

typedef RebolContextHandle RenContextHandle;
#define REN_CONTEXT_HANDLE_INVALID REBOL_CONTEXT_HANDLE_INVALID
#define REN_IS_CONTEXT_HANDLE_INVALID REBOL_IS_CONTEXT_HANDLE_INVALID

#else

CASSERT(0, hooks_h)

#endif

typedef char RenResult;

/*
// No return code convention yet... just 0 for success right now
// Cannot use ERROR! as this deals with init and shutdown of the code that
// carries Red Values.  The free takes a pointer and asks Red to put a value
// in it that it will recognize as invalid.
*/

RenResult RenAllocEngine(RenEngineHandle * engineOut);

RenResult RenFreeEngine(RenEngineHandle engine);



/*
// While Engines conceptually isolate one set of words from another in a
// sort of sandboxed way, a Context is merely a *binding* context within an
// Engine.  When symbols are loaded, they provide that implicit argument
// to bind.  System contexts or otherwise may be looked up by name.
*/

RenResult RenAllocContext(
    RenEngineHandle engine,
    RenContextHandle * contextOut
);

RenResult RenFreeContext(RenEngineHandle engine, RenContextHandle context);

RenResult RenFindContext(
    RenEngineHandle engine,
    char const * name,
    RenContextHandle *contextOut
);

RenResult RenGetEngineForContext(
    RenContextHandle context,
    RenEngineHandle * engineOut
); // TBD: should this be the client's responsibility to remember the engine?



/*
// Unified workhorse bridge function.  It can LOAD, splice blocks, evaluate
// without making a block out of the result, etc.  The two main tricks at
// work are that it accepts a pointer to an array of values which have a
// RedCell at the head but might be larger than a RedCell, and it uses an
// invalid RedCell bitpattern of TYPE_VALUE for instances of string text
// that need to be loaded.  If you do pass in a constructOut, it should
// have the datatype field already set in the header that you want.
//
// If the RedCell represents a series, then inside the guts of the hook it
// will remember that there is a reference being used by the binding.  A
// reference count will be added by the runtime.
//
// Should probably return a RedCell in order to give back rich errors.
*/

#define REN_SUCCESS 0
#define REN_ERROR_TOO_MANY_ARGS 1
#define REN_ERROR_NO_SUCH_CONTEXT 2
#define REN_BUFFER_TOO_SMALL 3

RenResult RenConstructOrApply(
    RenEngineHandle engine,
    RenContextHandle context,
    RenCell const * applicandPtr,
    RenCell * loadablesPtr, // should this be mutable, and be loaded?
    size_t numLoadables,
    size_t sizeofLoadable,
    RenCell * constructOutDatatypeIn,
    RenCell * applyOut
);


/*
// Every cell that needs it has to be released by the reference counting.
// There should be only one release per cell returned by RedConstructOrApply.
// This applies to any cells that came back via constructOut or applyOut.
//
// In case you have an array of cells to release for some reason at once,
// the API takes a number of cells.  Again, the size in bytes skipped can
// vary in case the cells are at the head of a block of data of a certain
// additional size needed by the binding.
*/

RenResult RenReleaseCells(
    RenEngineHandle engine,
    RenCell * cellsPtr,
    size_t numCells,
    size_t sizeofCellBlock
);


/*
// It's hard to know exactly where to draw the line in terms of offering core
// functionality as an API hook vs. using the generalized Apply.  But FORM
// is a very basic one that is needed everywhere...including iostream
// operators, string casting, and for debug output.
//
// This is the kind of API that takes a buffer size in, and tells you how
// many bytes the UTF-8 string needs.  If the number of bytes is greater than
// buffer, you get the first bufSize bytes and an error code warning you
// that you didn't get the whole string.  You can then call it again with a
// new buffer of the appropriate size.
*/

RenResult RenFormAsUtf8(
    RenEngineHandle engine,
    RenCell const * value,
    char * buffer,
    size_t bufSize,
    size_t * numBytesOut
);

#endif
