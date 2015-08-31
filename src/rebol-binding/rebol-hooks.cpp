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
            const REBVAL *error;

            // Unfortunate fact: setjmp and longjmp do not play well with
            // C++ or anything requiring stack-unwinding.
            //
            //     http://stackoverflow.com/a/1376099/211160
            //
            // This means an error has a good chance of skipping destructors
            // here, and really everything would need to be written in pure C
            // to be safe.

            PUSH_UNHALTABLE_TRAP(&error, &state);

// The first time through the following, 'error' will be NULL, but...
// `raise Error()` can longjmp here, 'error' won't be NULL *if* that happens!

            if (error) {
                if (VAL_ERR_NUM(error) == RE_HALT)
                    throw std::runtime_error {"Halt during initialization"};

                if (IS_ERROR(error))
                    throw std::runtime_error {
                        to_string(Value {*error, engine})
                    };
                else
                    throw std::runtime_error("!Error thrown in rebol-hooks");
            }

            Init_Task();    // Special REBOL values per task

            // Pop our error trapping state

            DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

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
            Val_Init_Object(contextOut, frame);
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
        REBVAL * extraOut
    ) {
        lazyThreadInitializeIfNeeded(engine);

        REBOL_STATE state;
        const REBVAL * error;

        // Note: No C++ allocations can happen between here and the POP_STATE
        // calls as long as the C stack is in control, as setjmp/longjmp will
        // subvert stack unwinding and just reset the processor state.

        PUSH_UNHALTABLE_TRAP(&error, &state);

        bool applying = false;
        bool is_aggregate_managed = false;
        REBSER * aggregate = Make_Array(numLoadables * 2);

// The first time through the following code 'error' will be NULL, but...
// `raise Error()` can longjmp here, 'error' won't be NULL *if* that happens!

        if (error) {
            if (!is_aggregate_managed)
                Free_Series(aggregate);

            if (VAL_ERR_NUM(error) == RE_HALT) {
                // cancellation in middle of interpretation from outside
                // the evaluation loop (e.g. Escape).
                return REN_EVALUATION_HALTED;
            }

            *extraOut = *error;

            return applying ? REN_APPLY_ERROR : REN_CONSTRUCT_ERROR;
        }

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

                // CAN raise errors and longjmp backwards on the C stack to
                // the `if (error)` case above!  These are the errors that
                // happen if the input is bad (unmatched parens, etc...)

                REBSER * transcoded = Scan_Source(
                    loadText, LEN_BYTES(loadText)
                );

                if (context) {
                    // Binding Do_String did by default...except it only
                    // worked with the user context.  Fell through to lib.

                    REBCNT len = VAL_OBJ_FRAME(context)->tail;

                    if (len > 0)
                        ASSERT_VALUE_MANAGED(BLK_HEAD(transcoded));

                    Bind_Values_All_Deep(
                        BLK_HEAD(transcoded),
                        VAL_OBJ_FRAME(context)
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

                // transcoded series is managed, can't free it...
            }
            else {
                // Just an ordinary value cell
                ASSERT_VALUE_MANAGED(cell);
                Append_Value(aggregate, cell);
            }

            current += sizeofLoadable;
        }

        RenResult result;

        if (constructOutDatatypeIn) {
            enum Reb_Kind resultType = VAL_TYPE(constructOutDatatypeIn);
            if (ANY_BLOCK(constructOutDatatypeIn)) {
                // They actually wanted a constructed value, and they wanted
                // effectively our aggregate...maybe with a different type.
                // Depending on how much was set in the "datatype in" we may
                // not have to rewrite the header bits (but Val_Inits do).

                Val_Init_Series(constructOutDatatypeIn, resultType, aggregate);

                // Val_Init makes aggregate a managed series, can't free it
                is_aggregate_managed = true;
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
                Val_Init_Object(constructOutDatatypeIn, frame);
            }
            else {
                // If they didn't want a block, then they better want the type
                // of the first thing in the block.  And there better be
                // something in that block.

                REBCNT count = SERIES_TAIL(aggregate);

                if (count != 1) {
                    // Requested construct, but a singular item didn't come
                    // back (either 0 or more than 1 element in aggregate)
                    Val_Init_Error(
                        extraOut,
                        Make_Error(
                            RE_MISC, // Make error code for this...
                            0,
                            0,
                            0
                        )
                    );
                    result = REN_CONSTRUCT_ERROR;
                    goto return_result;
                }
                else if (resultType != VAL_TYPE(BLK_HEAD(aggregate))) {
                    // Requested construct and value type was wrong
                    Val_Init_Error(
                        extraOut,
                        Make_Error(
                            RE_INVALID_ARG, // Make error code for this...
                            BLK_HEAD(aggregate),
                            0,
                            0
                        )
                    );
                    result = REN_CONSTRUCT_ERROR;
                    goto return_result;
                }
                else {
                    *constructOutDatatypeIn = *BLK_HEAD(aggregate);
                }
            }
        }

        if (applyOut) {
            applying = true;

            if (is_aggregate_managed) {
                // Must protect from GC before doing an evaluation...
                SAVE_SERIES(aggregate);
            }

            if (applicand) {
                result = Generalized_Apply(
                    applyOut, extraOut, applicand, aggregate, FALSE
                );
            }
            else {
                // Assume that nullptr for applicand means "just do the block
                // that was in the loadables".  This keeps us from having to
                // export a version of DO separately.

                if (Do_Block_Throws(applyOut, aggregate, 0)) {
                    TAKE_THROWN_ARG(extraOut, applyOut);
                    result = REN_APPLY_THREW;
                }
                else
                    result = REN_SUCCESS;
            }

            if (is_aggregate_managed)
                UNSAVE_SERIES(aggregate);
        }
        else
            result = REN_SUCCESS;

        // Pop our error trapping state, then unsave the aggregate (must be
        // done in this order)

    return_result:
        // We only free the series if we didn't manage it.  For simplicity
        // we control this with a boolean flag for now.
        assert(is_aggregate_managed == SERIES_GET_FLAG(aggregate, SER_MANAGED));
        if (!is_aggregate_managed)
            Free_Series(aggregate);

        DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

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
        Val_Init_String(&strValue, mo.series);

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

    RenResult ShimHalt() {
        Do_Error(TASK_HALT_ERROR);
        DEAD_END;
    }

    void ShimInitThrown(REBVAL *out, REBVAL const *value, REBVAL const *name) {
        *out = *name;
        CONVERT_NAME_TO_THROWN(out, value);
    }

    RenResult ShimRaiseError(REBVAL const * error) {
        Do_Error(error);
        DEAD_END;
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
    REBVAL * extraOut
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
        extraOut
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


RenResult RenShimHalt() {
    return ren::internal::hooks.ShimHalt();
}


void RenShimInitThrown(REBVAL *out, REBVAL const *value, REBVAL const *name) {
    ren::internal::hooks.ShimInitThrown(out, value, name);
}


RenResult RenShimRaiseError(RenCell const * error) {
    return ren::internal::hooks.ShimRaiseError(error);
}
