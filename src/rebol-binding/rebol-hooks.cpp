#ifndef NDEBUG
#include <unordered_set>
#include <unordered_map>
#endif
#include <cassert>
#include <stdexcept>

#include "rencpp/rebol.hpp"

// !!! hooks should not be throwing exceptions; still some in threadinit
#include "rencpp/error.hpp"

#include <utility>
#include <vector>
#include <algorithm>

#include <thread>

#include "rebol/src/include/sys-value.h"
#include "rebol/src/include/sys-state.h"

void Init_Task_Context();    // Special REBOL values per task


namespace ren {

namespace internal {

#ifndef NDEBUG
    std::unordered_map<
        decltype(RebolEngineHandle::data),
        std::unordered_map<REBSER const *, unsigned int>
    > nodes;
#endif

class RebolHooks {

private:
    RebolEngineHandle theEngine; // currently only support one "Engine"
    REBSER * allocatedContexts;


public:
    RebolHooks () :
        theEngine (REBOL_ENGINE_HANDLE_INVALID),
        allocatedContexts (nullptr)
    {
    }


//
// THREAD INITIALIZATION
//

//
// There are some thread-local pieces of state that need to be initialized
// or Rebol will complain.  However, we also want to be able to initialize
// on demand from whichever thread calls first.  This means we need to
// keep a list of which threads have been initialized and which not.
//
// !!! what if threads die and don't tell us?  Is there some part of
// the puzzle to ensure any thread that communicates with some form of
// allocation must come back with a free?
//

    std::vector<std::thread::id> threadsSeen;

    void lazyThreadInitializeIfNeeded(RebolEngineHandle engine) {
        if (
            std::find(
                threadsSeen.begin(),
                threadsSeen.end(),
                std::this_thread::get_id()
            )
            == std::end(threadsSeen)
        ) {
            // A misguided check in Rebol worries about the state of the CPU
            // stack.  If you've somehow hopped around so that the state
            // of the CPU stack it saw at its moment of initialization is
            // such that it is no longer at that same depth, it crashes
            // mysteriously even though nothing is actually wrong.  We beat
            // the CHECK_STACK by making up either a really big pointer or a
            // really small one for it to check against.  :-)

        #ifdef OS_STACK_GROWS_UP
            Stack_Limit = static_cast<void*>(-1);
        #else
            Stack_Limit = 0;
        #endif

            REBOL_STATE state;
            // Copied from c-do.c and Do_String.

            // Unfortunate fact #1: setjmp and longjmp do not play well with
            // C++ or anything requiring stack-unwinding.
            //
            //     http://stackoverflow.com/a/1376099/211160
            //
            // This means an error has a good chance of skipping destructors
            // here, and really everything would need to be written in pure C
            // to be safe.

            // Unfortunate fact #2, the real Halt_State used by QUIT is a global
            // shared between Do_String and the exiting function Halt_Code.  And
            // it's static to c-do.c - that has to be edited to communicate with
            // Rebol about things like QUIT or Ctrl-C.  (Quit could be replaced
            // with a new function, but evaluation interrupts can't.)

            PUSH_STATE(state, Halt_State);
            if (SET_JUMP(state)) {
                POP_STATE(state, Halt_State);
                Saved_State = Halt_State;
                Catch_Error(DS_NEXT); // Stores error value here
                REBVAL *val = Get_System(SYS_STATE, STATE_LAST_ERROR);
                *val = *DS_NEXT;
                if (VAL_ERR_NUM(val) == RE_QUIT) {
                    throw std::runtime_error {"Exit during initialization"};
                }
                if (VAL_ERR_NUM(val) == RE_HALT) {
                    throw std::runtime_error {"Halt during initialization"};
                }
                if (IS_ERROR(val))
                    throw std::runtime_error {
                        to_string(Value {*val, engine})
                    };
                else
                    throw std::runtime_error("!Error thrown in rebol-hooks");
            }
            SET_STATE(state, Halt_State);

            // Use this handler for both, halt conditions (QUIT, HALT) and error
            // conditions. As this is a top-level handler, simply overwriting
            // Saved_State is safe.
            Saved_State = Halt_State;

            Init_Task();    // Special REBOL values per task

            // Pop our error trapping state

            POP_STATE(state, Halt_State);
            Saved_State = Halt_State;

            threadsSeen.push_back(std::this_thread::get_id());
        }
    }

//
// ENGINE ALLOCATION AND FREEING
//

    RenResult AllocEngine(RebolEngineHandle * engineOut) {
        if (not (REBOL_IS_ENGINE_HANDLE_INVALID(theEngine)))
            throw std::runtime_error(
                "Rebol does not have Engine memory isolation at this"
                " point in time, and no VM sandboxing has been added"
                " in the binding.  So only one engine may be allocated"
            );

        runtime.lazyInitializeIfNecessary();

        // The root initialization initializes its thread-locals
        threadsSeen.push_back(std::this_thread::get_id());

        theEngine.data = 1020;
        *engineOut = theEngine;

        return REN_SUCCESS;
    }


    RenResult FreeEngine(RebolEngineHandle engine) {

        assert(engine.data == 1020);

        if (REBOL_IS_ENGINE_HANDLE_INVALID(engine))
            return REN_BAD_ENGINE_HANDLE;

        theEngine = REBOL_ENGINE_HANDLE_INVALID;

        return REN_SUCCESS;
    }



//
// CONTEXT FINDING
//


    RenResult FindContext(
        RebolEngineHandle engine,
        char const * name,
        RenCell * contextOut
    ) {
        lazyThreadInitializeIfNeeded(engine);

        assert(not REBOL_IS_ENGINE_HANDLE_INVALID(theEngine));
        assert(engine.data == theEngine.data);

        REBSER * frame = nullptr;
        if (strcmp(name, "USER") == 0)
            frame = VAL_OBJ_FRAME(Get_System(SYS_CONTEXTS, CTX_USER));
        else if (strcmp(name, "LIB") == 0)
            frame = VAL_OBJ_FRAME(Get_System(SYS_CONTEXTS, CTX_LIB));
        if (strcmp(name, "SYS") == 0)
            frame = VAL_OBJ_FRAME(Get_System(SYS_CONTEXTS, CTX_SYS));

        // don't expose CTX_ROOT?

        if (frame) {
            SET_OBJECT(contextOut, frame);
            return REN_SUCCESS;
        }

        return REN_ERROR_NO_SUCH_CONTEXT;
    }



//
// CONSTRUCT OR APPLY HOOK
//

    //
    // The ConstructOrApply hook was designed to be a primitive that
    // allows for efficiency in calling the "Generalized Apply" from
    // higher level languages like C++.  See notes in runtime.hpp
    //
    // Though the primitive was designed for efficient processing without
    // the need to create series unnecessarily, it is currently in a
    // "just get the contract settled" state.  Optimizing would likely
    // best be done by parameterizing the Rebol runtime functions directly
    //

    RenResult ConstructOrApply(
        RebolEngineHandle engine,
        REBVAL const * context,
        REBVAL const * applicand,
        REBVAL const * loadablesPtr,
        size_t numLoadables,
        size_t sizeofLoadable,
        REBVAL * constructOutDatatypeIn,
        REBVAL * applyOut,
        REBVAL * errorOut
    ) {
        lazyThreadInitializeIfNeeded(engine);

        REBOL_STATE state;

        bool applying = false;
        REBSER * aggregate = Make_Block(numLoadables * 2);

        SAVE_SERIES(aggregate);

        // Note: No C++ allocations can happen between here and the POP_STATE
        // calls as long as the C stack is in control, as setjmp/longjmp will
        // subvert stack unwinding and just reset the processor state.

        PUSH_STATE(state, Halt_State);
        if (SET_JUMP(state)) {
            POP_STATE(state, Halt_State);
            Saved_State = Halt_State;

            UNSAVE_SERIES(aggregate);

            Catch_Error(DS_NEXT); // Stores error value here
            REBVAL *val = Get_System(SYS_STATE, STATE_LAST_ERROR);
            *val = *DS_NEXT;

            if (VAL_ERR_NUM(val) == RE_QUIT) {
                // Cancellation to exit to the OS with an error code number,
                // purposefully requested by the programmer
                *errorOut = *VAL_ERR_VALUE(DS_NEXT);
                return REN_EVALUATION_EXITED;
            }

            if (VAL_ERR_NUM(val) == RE_HALT) {
                // cancellation in middle of interpretation from outside
                // the evaluation loop (e.g. Escape)
                return REN_EVALUATION_CANCELLED;
            }

            // Some other generic error; it may have occurred during the
            // construct phase or the apply phase
            *errorOut = *val;

            return applying ? REN_APPLY_ERROR : REN_CONSTRUCT_ERROR;
        }
        SET_STATE(state, Halt_State);

        // Use this handler for both, halt conditions (QUIT, HALT) and error
        // conditions. As this is a top-level handler, simply overwriting
        // Saved_State is safe.
        Saved_State = Halt_State;

        int result = REN_SUCCESS;

        if (applicand) {
            // This is the current rule and the code expects it to be true,
            // but if it were not what might it mean?  This would be giving
            // a value but not asking for a result.  It's free to return
            // the result so this would never be done for performance.
            assert(applyOut);
        }

        // We don't necessarily have a pointer to an array of REBVALs if
        // sizeofLoadable != sizeof(REBVAL); so keep "current" as char*
        auto current = reinterpret_cast<char const *>(loadablesPtr);

        // For the initial state of the binding we'll focus on correctness
        // instead of optimization.  That means we'll take the "loadables"
        // and form a block out of them--even when we weren't asked to,
        // rather than implement a more efficient form of enumeration.

        // If we were asked to construct a block type, then this will be
        // the block we return...as there was no block indicator in the
        // initial string.  If we were asking to construct a non-block type,
        // then it should be the first element in this block.

        for (size_t index = 0; index < numLoadables; index++) {

            auto cell = const_cast<REBVAL *>(
                reinterpret_cast<volatile REBVAL const *>(current)
            );

            if (VAL_TYPE(cell) == REB_END) {

                // This is our "Alien" type that wants to get loaded.  Key
                // to his loading problem is that he wants to know whether
                // he is an explicit or implicit block type.  So that means
                // discerning between "foo bar" and "[foo bar]", which we
                // get through transcode which returns [foo bar] and
                // [[foo bar]] that discern the cases

                auto loadText = reinterpret_cast<REBYTE*>(
                    VAL_HANDLE_DATA(cell)
                );

                // CAN Throw_Error! if the input is bad (unmatched parens,
                // etc...

                REBSER * transcoded = Scan_Source(
                    loadText, LEN_BYTES(loadText)
                );

                if (context) {
                    // Binding Do_String did by default...except it only
                    // worked with the user context.  Fell through to lib.

                    REBCNT len = VAL_OBJ_FRAME(context)->tail;

                    Bind_Block(
                        VAL_OBJ_FRAME(context),
                        BLK_HEAD(transcoded),
                        BIND_ALL | BIND_DEEP
                    );

                    REBVAL vali;
                    SET_INTEGER(&vali, len);

                    Resolve_Context(
                        VAL_OBJ_FRAME(context), Lib_Context, &vali, FALSE, 0
                    );
                }

                // Might think to use Append_Block here, but it's under
                // an #ifdef and apparently unused.  This is its definition.

                Insert_Series(
                    aggregate,
                    aggregate->tail,
                    reinterpret_cast<REBYTE*>(BLK_HEAD(transcoded)),
                    transcoded->tail
                );
            }
            else {
                // Just an ordinary value cell
                Append_Val(aggregate, cell);
            }

            current += sizeofLoadable;
        }

        if (applyOut) {
            applying = true;
            if (applicand) {
                result = Generalized_Apply(
                    const_cast<REBVAL *>(applicand),
                    const_cast<REBSER *>(aggregate),
                    FALSE,
                    errorOut
                );
                // even if there was an error, we need to keep going and
                // safely clean things up.
                if (result == REN_SUCCESS)
                    *applyOut = *DS_TOP;
                else
                    SET_UNSET(applyOut);
            }
            else {
                // Assume that nullptr for applicand means "just do the block
                // that was in the loadables".  This keeps us from having to
                // export a version of DO separately.

                *applyOut = *Do_Blk(aggregate, 0); // result is volatile
            }
        }

        if (constructOutDatatypeIn) {
            applying = false;
            REBOL_Types resultType = static_cast<REBOL_Types>(
                VAL_TYPE(constructOutDatatypeIn)
            );
            if (ANY_BLOCK(constructOutDatatypeIn)) {
                // They actually wanted a constructed value, and they wanted
                // effectively our aggregate...maybe with a different type.
                // Depending on how much was set in the "datatype in" we may
                // not have to rewrite the header bits, as Set_Series does.
                Set_Series(resultType, constructOutDatatypeIn, aggregate);
            }
            else if (IS_OBJECT(constructOutDatatypeIn)) {
                // They want to create a "Context"; so we need to execute
                // the aggregate as per Make_Object.  First parameter to
                // Make_Object is the parent object series.  At the moment
                // we don't have an interface to give a context a parent
                // from RenCpp

                // Once again, the REBVAL that is taken as "block" isn't a
                // block value, but the value pointer at the *head* of
                // the block.  :-/

                REBSER * frame = Make_Object(nullptr, BLK_HEAD(aggregate));

                // This sets REB_OBJECT in the header, possibly redundantly
                Set_Object(constructOutDatatypeIn, frame);
            }
            else {
                // If they didn't want a block, then they better want the type
                // of the first thing in the block.  And there better be
                // something in that block.

                REBCNT len = BLK_LEN(aggregate);

                if (len == 0) {
                    // Requested construct, but no value came back.
                    VAL_SET(errorOut, REB_NONE); // improve?
                    return REN_CONSTRUCT_ERROR;
                }

                if (len > 1) {
                    // Requested construct and more than one value
                    VAL_SET(errorOut, REB_NONE); // improve?
                    return REN_CONSTRUCT_ERROR;
                }

                *constructOutDatatypeIn = *BLK_HEAD(aggregate);

                if (resultType != VAL_TYPE(constructOutDatatypeIn)) {
                    // Requested construct and value type was wrong
                    VAL_SET(errorOut, REB_NONE); // improve?
                    return REN_CONSTRUCT_ERROR;
                }
            }
        }

        // Pop our error trapping state, then unsave the aggregate (must be
        // done in this order)

        POP_STATE(state, Halt_State);
        Saved_State = Halt_State;

        UNSAVE_SERIES(aggregate);

        return result;
    }

    RenResult ReleaseCells(
        RebolEngineHandle engine,
        REBVAL const * valuesPtr,
        size_t numValues,
        size_t sizeofValue
    ) {
        lazyThreadInitializeIfNeeded(engine);

        auto current = reinterpret_cast<char const *>(valuesPtr);
        for (size_t index = 0; index < numValues; index++) {
            auto cell = const_cast<REBVAL *>(
                reinterpret_cast<REBVAL const *>(current)
            );

        #ifndef NDEBUG
            assert(ANY_SERIES(cell));
            auto it = nodes[engine.data].find(VAL_SERIES(cell));
            assert(it != nodes[engine.data].end());

            it->second--;
            if (it->second == 0) {
                size_t numErased = nodes[engine.data].erase(VAL_SERIES(cell));
                assert(numErased == 1);
                if (nodes[engine.data].empty())
                    nodes.erase(engine.data);
            }
        #endif

            current += sizeofValue;
        }

        return REN_SUCCESS;
    }


    RenResult FormAsUtf8(
        RebolEngineHandle engine,
        REBVAL const * value,
        unsigned char * buffer,
        size_t bufSize,
        size_t * numBytesOut
    ) {
        lazyThreadInitializeIfNeeded(engine);

#ifndef NDEBUG
        if (ANY_SERIES(value)) {
            auto it = nodes.find(engine.data);
            assert(it != nodes.end());
            assert(
                it->second.find(reinterpret_cast<REBSER const *>(
                    VAL_SERIES(value))
                )
                != it->second.end()
            );
        }
#endif

        // First we mold with the "FORM" settings and get a STRING!
        // series out of that.

        REB_MOLD mo;
        mo.series = nullptr;
        mo.opts = 0;
        mo.indent = 0;
        mo.period = 0;
        mo.dash = 0;
        mo.digits = 0;
        Reset_Mold(&mo);
        Mold_Value(&mo, const_cast<REBVAL *>(value), 0);


        // Now that we've got our STRING! we need to encode it as UTF8 into
        // a "shared buffer".  How is that that TO conversions use a shared
        // buffer and then Set_Series to that, without having a problem?
        // Who knows, but we've got our own buffer so that's not important.

        REBVAL strValue;
        Set_Series(REB_STRING, &strValue, mo.series);

        REBSER * utf8 = Encode_UTF8_Value(&strValue, VAL_LEN(&strValue), 0);


        // Okay that should be the UTF8 data.  Let's copy it into the buffer
        // the caller sent us.

        REBCNT len = SERIES_LEN(utf8) - 1;
        *numBytesOut = static_cast<size_t>(len);

        RenResult result;
        if (len > bufSize) {
            len = bufSize;
            // should copy portion of buffer in anyway
            result = REN_BUFFER_TOO_SMALL;
        }
        else {
            result = REN_SUCCESS;
        }

        for (REBCNT index = 0; index < len; index++)
            buffer[index] = SERIES_DATA(utf8)[index];

        return result;
    }

    RenResult ShimCancel() {
        Halt_Code(RE_HALT, 0);
        UNREACHABLE_CODE();
    }

    RenResult ShimExit(int status) {
        REBVAL value;
        SET_INTEGER(&value, status);
        Halt_Code(RE_QUIT, &value);
        UNREACHABLE_CODE();
    }

    RenResult ShimRaiseError(REBVAL const * error) {
        Throw_Error(VAL_SERIES(error));
        return REN_SUCCESS; // never happens...
    }

    ~RebolHooks () {
        assert(nodes.empty());
    }
};

RebolHooks hooks;

} // end namespace internal

} // end namespace ren


RenResult RenAllocEngine(RebolEngineHandle * engineOut) {
    return ren::internal::hooks.AllocEngine(engineOut);
}


RenResult RenFreeEngine(RebolEngineHandle engine) {
    return ren::internal::hooks.FreeEngine(engine);
}


RenResult RenFindContext(
    RenEngineHandle engine,
    char const * name,
    REBVAL * contextOut
) {
    return ren::internal::hooks.FindContext(engine, name, contextOut);
}


RenResult RenConstructOrApply(
    RebolEngineHandle engine,
    REBVAL const * context,
    REBVAL const * valuePtr,
    REBVAL const * loadablesPtr,
    size_t numLoadables,
    size_t sizeofLoadable,
    REBVAL * constructOutTypeIn,
    REBVAL * applyOut,
    REBVAL * errorOut
) {
    return ren::internal::hooks.ConstructOrApply(
        engine,
        context,
        valuePtr,
        loadablesPtr,
        numLoadables,
        sizeofLoadable,
        constructOutTypeIn,
        applyOut,
        errorOut
    );

}


RenResult RenReleaseCells(
    RebolEngineHandle engine,
    REBVAL const * valuesPtr,
    size_t numValues,
    size_t sizeofValue
) {
    return ren::internal::hooks.ReleaseCells(
        engine, valuesPtr, numValues, sizeofValue
    );
}


RenResult RenFormAsUtf8(
    RenEngineHandle engine,
    RenCell const * value,
    unsigned char * buffer,
    size_t bufSize,
    size_t * lengthOut
) {
    return ren::internal::hooks.FormAsUtf8(
        engine, value, buffer, bufSize, lengthOut
    );
}


RenResult RenShimCancel() {
    return ren::internal::hooks.ShimCancel();
}


RenResult RenShimExit(int status) {
    return ren::internal::hooks.ShimExit(status);
}

RenResult RenShimRaiseError(RenCell const * error) {
    return ren::internal::hooks.ShimRaiseError(error);
}
