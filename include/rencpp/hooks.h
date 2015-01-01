#ifndef RENCPP_HOOKS_H
#define RENCPP_HOOKS_H

/*
 * hooks.h
 * This file is part of RenCpp
 * Copyright (C) 2015 HostileFork.com
 *
 * Licensed under the Boost License, Version 1.0 (the "License")
 *
 *      http://www.boost.org/LICENSE_1_0.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.  See the License for the specific language governing
 * permissions and limitations under the License.
 *
 * See http://rencpp.hostilefork.com for more information on this project
 */


/*
 * For the sake of the thought experiment, this file a generalized one that
 * can be built with either C or C++.  One might consider how much work could
 * potentially be reused between the two bindings.
 */


/*
 * int32_t is in #include <stdint.h>, generally you should only use the
 * specific size definitions when doing external interfaces like this.
 *
 * http: *stackoverflow.com/q/6144682/211160
 */
#include <stdint.h>


/*
 * Portable compile-time assert for C
 * http: *stackoverflow.com/a/809465/211160
 */
#define CASSERT(predicate, file) _impl_CASSERT_LINE(predicate,__LINE__,file)
#define _impl_PASTE(a,b) a##b
#define _impl_CASSERT_LINE(predicate, line, file) \
    typedef char _impl_PASTE(assertion_failed_##file##_,line)[2*!!(predicate)-1];


/*
 * Return codes used by the binding.  They start at 10 at present due to
 * the hack to use the equal value of R_RETURN and REN_SUCCESS to avoid
 * trying to force Red to have R_XXX return conventions from RenShimPointer
 */
typedef int RenResult;
#define REN_SUCCESS 0
#define REN_ERROR_TOO_MANY_ARGS 10
#define REN_ERROR_NO_SUCH_CONTEXT 11
#define REN_BUFFER_TOO_SMALL 12
#define REN_SHIM_INITIALIZED 13


/*
 * Defines for the runtimes.  Strings are awkward for the preprocessor and
 * can only be used in a limited sense.
 */
#define REN_RUNTIME_RED 304
#define REN_RUNTIME_REBOL 1020


#ifndef REN_RUNTIME

/*
 * Need to define REN_RUNTIME, ensure you built CMake with either:
 * -DRUNTIME=rebol or -DRUNTIME=red
 */
CASSERT(0, hooks_h)

#elif REN_RUNTIME == REN_RUNTIME_RED
/*
 * RedCell
 *
 * Sometimes this fixed-size cell holds a value in its entirety--such as
 * an integer! or a date!.  For variable sized types such as series, the
 * values are only references to the data.  The cells thus contain a pointer
 * to the NODE! where the actual data is stored.  Also in the cell is an
 * index of where the reference is offset into the series.
 *
 * If a value contains a NODE! as a reference to the *actual* data, then that
 * is an abstraction that is good for the lifetime of the series.  Modulo
 * garbage collection, the 128-bit sequence is guaranteed to represent the
 * same series for all time.  The content of the series may change, the
 * embedded index may "go stale", but will not crash.  Look at:
 *
 *   https: *github.com/red/red/blob/master/runtime/allocator.reds
 *
 * It tells us the bits are laid out like this:
 *
 * ;-- cell header bits layout --
 * ;   31:     lock           ;-- lock series for active thread access only
 * ;   30:     new-line       ;-- new-line (LF) marker (before the slot)
 * ;   29-25:  arity          ;-- arity for routine! functions.
 * ;   24:     self?          ;-- self-aware context flag
 * ;   23:     node-body      ;-- op! body points to a block node
 * ;                          ;--     (instead of native code)
 * ;   22-8:   <reserved>
 * ;   7-0:    datatype ID    ;-- datatype number
 *
 * cell!: alias struct! [
 *    header  [integer!]      ;-- cell's header flags
 *    data1   [integer!]      ;-- placeholders to make a 128-bit cell
 *    data2   [integer!]
 *    data3   [integer!]
 * ]
 *
 * More specific interpretations of the 128 bits are given in the file:
 *
 *   https: *github.com/red/red/blob/master/runtime/datatypes/structures.reds
 *
 */

struct RedCell {
    int32_t header;
    int32_t data1;

    /*
     * C and C++ compilers actually have something called the "strict aliasing
     * requirement".  There are a lot of common mistakes with it:
     *
     *     http: *stackoverflow.com/questions/98650/
     *
     * Long story short: if a type was of one kind, you cannot reliably cast
     * it to another data type and read or write from it (unless the type you
     * cast to was char*, it's an exception).  When optimizing and being
     * concerned about whether to re-fetch a variable or use the last value,
     * it is driven by knowledge of the type system; every write to every
     * type does not cause it to assume anything in the world could have
     * changed.  For this reason we can't just cast one struct to another.
     */

    /*
     * For union initializer list syntax, see here:
     *     http: *stackoverflow.com/questions/18411039/
     */
    union {
        double dataD;
        struct {
            int32_t data2;
            int32_t data3;
        } s;
    };
};

struct RedEngineHandle {
    int data;
};
const struct RedEngineHandle RED_ENGINE_HANDLE_INVALID = {-1};
#define RED_IS_ENGINE_HANDLE_INVALID(handle) \
    ((handle).data == RED_ENGINE_HANDLE_INVALID.data)

struct RedContextHandle {
    void * pointer;
};
const struct RedContextHandle RED_CONTEXT_HANDLE_INVALID = {nullptr};
#define RED_IS_CONTEXT_HANDLE_INVALID(handle) \
    ((handle).pointer == RED_CONTEXT_HANDLE_INVALID.pointer)


/**
 ** MAP RED TYPES TO REN EQUIVALENTS
 **/

typedef RedCell RenCell;

typedef RedEngineHandle RenEngineHandle;
#define REN_ENGINE_HANDLE_INVALID RED_ENGINE_HANDLE_INVALID
#define REN_IS_ENGINE_HANDLE_INVALID RED_IS_ENGINE_HANDLE_INVALID

typedef RedContextHandle RenContextHandle;
#define REN_CONTEXT_HANDLE_INVALID RED_CONTEXT_HANDLE_INVALID
#define REN_IS_CONTEXT_HANDLE_INVALID RED_IS_CONTEXT_HANDLE_INVALID

/*
 * The Red runtime is still fake for the moment, so no real convention for the
 * stack has been established.  But this lays out what is needed for the binding
 * to be able to process arguments, if the RenShimPointer function signature is
 * as written.
 */
#define REN_STACK_RETURN(stack) \
    static_cast<RenCell *>((stack), nullptr)

#define REN_STACK_ARGUMENT(stack, index) \
    static_cast<RenCell *>((stack), nullptr)

#define REN_STACK_SHIM(stack) \
    static_cast<RenShimPointer>((stack), nullptr)

#elif REN_RUNTIME == REN_RUNTIME_REBOL

/*
 * While Red has no C headers for us to include (being written in Red and
 * Red/System), Rebol does.
 */
#include "rebol/src/include/sys-core.h"
#include "rebol/src/include/reb-ext.h"
#include "rebol/src/include/reb-lib.h"


/*
 * REBVAL is a typedef for Reb_Value, both defined in src/include/sys_value.h
 *
 * The Rebol codebase is (circa 2015) not able to build with a C++ compiler,
 * although patches do exist to make it possible.  Which is okay because you
 * can link the C and C++ .o files together from the separate builds.  Here
 * we are only sharing header files and macros, and linking through extern "C"
 */

struct RebolEngineHandle {
    int data;
};
const struct RebolEngineHandle REBOL_ENGINE_HANDLE_INVALID = {-1};
#define REBOL_IS_ENGINE_HANDLE_INVALID(handle) \
    ((handle).data == REBOL_ENGINE_HANDLE_INVALID.data)

struct RebolContextHandle {
    REBSER * series;
};
const struct RebolContextHandle REBOL_CONTEXT_HANDLE_INVALID = {nullptr};
#define REBOL_IS_CONTEXT_HANDLE_INVALID(handle) \
    ((handle).series == REBOL_CONTEXT_HANDLE_INVALID.series)

/**
 ** MAP REBOL TYPES TO REN EQUIVALENTS
 **/

typedef REBVAL RenCell;

typedef RebolEngineHandle RenEngineHandle;
#define REN_ENGINE_HANDLE_INVALID REBOL_ENGINE_HANDLE_INVALID
#define REN_IS_ENGINE_HANDLE_INVALID REBOL_IS_ENGINE_HANDLE_INVALID

typedef RebolContextHandle RenContextHandle;
#define REN_CONTEXT_HANDLE_INVALID REBOL_CONTEXT_HANDLE_INVALID
#define REN_IS_CONTEXT_HANDLE_INVALID REBOL_IS_CONTEXT_HANDLE_INVALID


/*
 * Although the abstraction is that the RenShimPointer returns a RenResult,
 * the native function pointers in Rebol do not do this.  It just happens
 * that 0 maps to R_RET, which is what we want.  The return conventions of
 * Red are unlikely to match that...and maybe never give back an integer
 * at all.  For the moment though we'll assume it does but the interpretation
 * as some kind of error code for the hook seems more sensible.
 */
CASSERT(REN_SUCCESS == R_RET, hooks_h)


/*
 * Here we have our definitions for how to find the relevant values on the
 * stack.  Because Rebol uses 1 based calculations, we have to add 1.  The
 * shim accessor here gets the actual pointer out vs. returning the whole
 * cell pointer for the function, which is not as general but if the stack
 * in Red winds up being lighter weight and not wanting to put the whole
 * 128 bit cell on then it might be more useful.
 */
#define REN_STACK_RETURN(stack) DSF_RETURN(stack - DS_Base)
#define REN_STACK_ARGUMENT(stack, index) DSF_ARGS(stack - DS_Base, (index + 1))
#define REN_STACK_SHIM(stack) VAL_FUNC_CODE(DSF_FUNC(stack - DS_Base))

#else

CASSERT(0, hooks_h)

#endif


typedef RenResult (* RenShimPointer)(RenCell * stack);


/*
 * Cannot use ERROR! as this deals with init and shutdown of the code that
 * carries Red Values.  The free takes a pointer and asks Red to put a value
 * in it that it will recognize as invalid.
 */

RenResult RenAllocEngine(RenEngineHandle * engineOut);

RenResult RenFreeEngine(RenEngineHandle engine);



/*
 * While Engines conceptually isolate one set of words from another in a
 * sort of sandboxed way, a Context is merely a *binding* context within an
 * Engine.  When symbols are loaded, they provide that implicit argument
 * to bind.  System contexts or otherwise may be looked up by name.
 */

RenResult RenAllocContext(
    RenEngineHandle engine,
    RenContextHandle * contextOut
);

RenResult RenFreeContext(RenEngineHandle engine, RenContextHandle context);

RenResult RenFindContext(
    RenEngineHandle engine,
    char const * name,
    RenContextHandle * contextOut
);

#ifdef RUNTIMES_MUST_FETCH_ENGINE_GIVEN_CONTEXT

/*
 * Binding keeps track of engine for every value and can always use the right
 * one, but should the runtime have to be able to reverse map them?
 *
 *     https: *github.com/hostilefork/rencpp/issues/16
 */

RenResult RenGetEngineForContext(
    RenContextHandle context,
    RenEngineHandle * engineOut
);

#endif


/*
 * Unified workhorse bridge function.  It can LOAD, splice blocks, evaluate
 * without making a block out of the result, etc.  The two main tricks at
 * work are that it accepts a pointer to an array of values which have a
 * RedCell at the head but might be larger than a RedCell, and it uses an
 * invalid RedCell bitpattern of TYPE_VALUE for instances of string text
 * that need to be loaded.  If you do pass in a constructOut, it should
 * have the datatype field already set in the header that you want.
 *
 * If the RedCell represents a series, then inside the guts of the hook it
 * will remember that there is a reference being used by the binding.  A
 * reference count will be added by the runtime.
 *
 * Should probably return a RedCell in order to give back rich errors.
 */

RenResult RenConstructOrApply(
    RenEngineHandle engine,
    RenContextHandle context,
    RenCell const * applicand,
    RenCell loadables[],
    size_t numLoadables,
    size_t sizeofLoadable,
    RenCell * constructOutDatatypeIn,
    RenCell * applyOut
);


/*
 * Every cell that needs it has to be released by the reference counting.
 * There should be only one release per cell returned by RedConstructOrApply.
 * This applies to any cells that came back via constructOut or applyOut.
 *
 * In case you have an array of cells to release for some reason at once,
 * the API takes a number of cells.  Again, the size in bytes skipped can
 * vary in case the cells are at the head of a block of data of a certain
 * additional size needed by the binding.
 */

RenResult RenReleaseCells(
    RenEngineHandle engine,
    RenCell cells[],
    size_t numCells,
    size_t sizeofCellBlock
);


/*
 * It's hard to know exactly where to draw the line in terms of offering core
 * functionality as an API hook vs. using the generalized Apply.  But FORM
 * is a very basic one that is needed everywhere...including iostream
 * operators, string casting, and for debug output.
 *
 * This is the kind of API that takes a buffer size in, and tells you how
 * many bytes the UTF-8 string needs.  If the number of bytes is greater than
 * buffer, you get the first bufSize bytes and an error code warning you
 * that you didn't get the whole string.  You can then call it again with a
 * new buffer of the appropriate size.
 */

RenResult RenFormAsUtf8(
    RenEngineHandle engine,
    RenCell const * value,
    char * buffer,
    size_t bufSize,
    size_t * numBytesOut
);

#endif
