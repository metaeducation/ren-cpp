#ifndef RENCPP_HOOKS_H
#define RENCPP_HOOKS_H

/*
 * hooks.h
 * This file is part of RenCpp
 * Copyright (C) 2015-2017 HostileFork.com
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
 * http://stackoverflow.com/q/6144682/211160
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
 * the hack to use the equal value of R_OUT and REN_SUCCESS to avoid
 * trying to force Red to have R_XXX return conventions from RenShimPointer
 */
#define REN_SUCCESS 5 /* same value as R_OUT */
#define REN_APPLY_THREW 1 /* same value as R_OUT_THREW */

#define REN_CONSTRUCT_ERROR 10
#define REN_APPLY_ERROR 11
#define REN_ERROR_NO_SUCH_CONTEXT 13
#define REN_BUFFER_TOO_SMALL 14
#define REN_SHIM_INITIALIZED 15
#define REN_EVALUATION_HALTED 16
#define REN_BAD_ENGINE_HANDLE 17


/*
 * The RenResult does double duty as the result code from functions and the
 * Rebol NATIVE! function return type, so it has to match the latter.  Rebol
 * uses fixed size types to get <stdint.h>-style compatibility.  This may
 * have to be adapted for Red, which also may have an entirely different
 * prototype needed for shims speaking the stack protocol.
 */
typedef uint32_t RenResult; // REBCNT-compatible


/*
 * The original concept of Ren-Cpp was to work with raw REBVAL or Red cells,
 * which are structs that are 4 platform pointers in size.  The practical
 * issues of GC extension for this model, included with the advent of
 * "singular" REBSER nodes that could hold a single value, led to switching
 * so that the responsibility for the cell's memory is on the engine side.
 *
 * In order to avoid pulling in all of the Rebol includes to every client
 * of Ren/C++, we make an "opaque type":
 *
 * https://en.wikipedia.org/wiki/Opaque_data_type
 *
 * Rebol values are four times the size of the platform word size.
 */

typedef struct {
    uintptr_t data[4];
} RenCell; // actually a REBSER node--a "singular array" w/1 REBVAL


typedef struct {
    int data;
} RebolEngineHandle;

const RebolEngineHandle REBOL_ENGINE_HANDLE_INVALID = {-1};
#define REBOL_IS_ENGINE_HANDLE_INVALID(handle) \
    ((handle).data == REBOL_ENGINE_HANDLE_INVALID.data)


/**
 ** MAP REBOL TYPES TO REN EQUIVALENTS
 **/

typedef RebolEngineHandle RenEngineHandle;
#define REN_ENGINE_HANDLE_INVALID REBOL_ENGINE_HANDLE_INVALID
#define REN_IS_ENGINE_HANDLE_INVALID REBOL_IS_ENGINE_HANDLE_INVALID


/*
 * If the evaluator is cancelled by a signal from outside, and the exception
 * makes it to the shim, it will be processed by this function in the shim
 */
RenResult RenShimHalt();


/*
 * When a throw happens, it has two RenCells to work with...the thrown value
 * and a value representing a label.  They can't both fit into a single
 * RenCell of size for the function's output slot, so some lookaside is
 * needed.  This initializes a value to indicate both parts of the result.
 */
void RenShimInitThrown(RenCell *out, RenCell const *value, RenCell const *name);


/*
 * Like RenShimExit but when an error happens.  In Rebol, at least, the return
 * value will be ignored because the routine longjmps as an "exception"
 */
RenResult RenShimFail(RenCell const * error);


/*
 * Cannot use ERROR! as this deals with init and shutdown of the code that
 * carries Red Values.  The free takes a pointer and asks Red to put a value
 * in it that it will recognize as invalid.
 */

RenResult RenAllocEngine(RenEngineHandle * out);

RenResult RenFreeEngine(RenEngineHandle engine);



/*
 * While Engines conceptually isolate one set of words from another in a
 * sort of sandboxed way, a Context is merely a *binding* context within an
 * Engine.  When symbols are loaded, they provide that implicit argument
 * to bind.  System contexts or otherwise may be looked up by name.
 */

RenResult RenFindContext(
    RenCell * out,
    RenEngineHandle engine,
    char const * name
);


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
 */

RenResult RenConstructOrApply(
    RenEngineHandle engine,
    RenCell const * context,
    RenCell const * applicand,
    RenCell * const * loadablesCell,
    size_t numLoadables,
    size_t sizeofLoadable,
    RenCell * constructOutDatatypeIn,
    RenCell * applyOut,
    RenCell * errorOut
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
    RenCell const * cell,
    unsigned char * buffer,
    size_t bufSize,
    size_t * numBytesOut
);

#endif
