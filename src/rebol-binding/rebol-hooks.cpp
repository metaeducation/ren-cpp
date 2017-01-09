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

#include "rebol-common.hpp"


namespace ren {

namespace internal {

std::mutex refcountMutex;
ren::AnyValue * head;

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
// ENGINE ALLOCATION AND FREEING
//

    RenResult AllocEngine(RebolEngineHandle * engineOut) {
        if (not (REBOL_IS_ENGINE_HANDLE_INVALID(theEngine)))
            throw std::runtime_error(
                "Rebol does not have Engine memory isolation at this"
                " point in time, and no VM sandboxing has been added"
                " in the binding.  So only one engine may be allocated"
            );

        theEngine.data = 1020;

        runtime.lazyInitializeIfNecessary();

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
        RenCell * out,
        RebolEngineHandle engine,
        char const * name
    ) {
        assert(not REBOL_IS_ENGINE_HANDLE_INVALID(theEngine));
        assert(engine.data == theEngine.data);

        REBCTX * context = nullptr;
        if (strcmp(name, "USER") == 0)
            context = VAL_CONTEXT(Get_System(SYS_CONTEXTS, CTX_USER));
        else if (strcmp(name, "LIB") == 0)
            context = VAL_CONTEXT(Get_System(SYS_CONTEXTS, CTX_LIB));
        if (strcmp(name, "SYS") == 0)
            context = VAL_CONTEXT(Get_System(SYS_CONTEXTS, CTX_SYS));

        // don't expose CTX_ROOT?

        if (context) {
            Init_Object(AS_REBVAL(out), context);
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
        REBVAL * const * loadablesPtr,
        size_t numLoadables,
        size_t sizeofLoadable,
        REBVAL * constructOutDatatypeIn,
        REBVAL * applyOut,
        REBVAL * extraOut
    ) {
        assert(engine.data == 1020);

        struct Reb_State state;
        REBCTX * error;

        // longjmp could "clobber" this variable if it were not volatile, and
        // code inside of the `if (error)` depends on possible modification
        // between the setjmp (PUSH_UNHALTABLE_TRAP) and the longjmp
        volatile bool applying = false;

        PUSH_UNHALTABLE_TRAP(&error, &state);

// The first time through the following code 'error' will be NULL, but...
// `fail` can longjmp here, 'error' won't be NULL *if* that happens!

        if (error) {
            // do not need to free series... it is done automatically

            if (ERR_NUM(error) == RE_HALT) {
                // cancellation in middle of interpretation from outside
                // the evaluation loop (e.g. Escape).
                return REN_EVALUATION_HALTED;
            }

            Init_Error(extraOut, error);

            return applying ? REN_APPLY_ERROR : REN_CONSTRUCT_ERROR;
        }

        // Note: No C++ allocations can happen between here and the POP_STATE
        // calls as long as the C stack is in control, as setjmp/longjmp will
        // subvert stack unwinding and just reset the processor state.

        bool is_aggregate_managed = false;
        REBARR * aggregate = Make_Array(numLoadables * 2);

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

            auto cell = *reinterpret_cast<REBVAL * const *>(current);

            if (VAL_TYPE_RAW(cell) == REB_0) {

                // This is our "Alien" type that wants to get loaded (voids
                // cannot be legally loaded into blocks, by design).  Key
                // to his loading problem is that he wants to know whether
                // he is an explicit or implicit block type.  So that means
                // discerning between "foo bar" and "[foo bar]", which we
                // get through transcode which returns [foo bar] and
                // [[foo bar]] that discern the cases

                auto loadText = reinterpret_cast<REBYTE*>(
                    cell->payload.handle.pointer // not actually a REB_HANDLE
                );

                // !!! Temporary: we can't let the GC see a REB_0 trash.
                // There will be a REB_LOADABLE and ET_LOADABLE type, so use
                // that when it arrives, but until then blank it.
                //
                SET_BLANK(cell);

                // CAN raise errors and longjmp backwards on the C stack to
                // the `if (error)` case above!  These are the errors that
                // happen if the input is bad (unmatched parens, etc...)

                REBARR * transcoded = Scan_UTF8_Managed(
                    loadText, LEN_BYTES(loadText)
                );

                if (context) {
                    // Binding Do_String did by default...except it only
                    // worked with the user context.  Fell through to lib.

                    REBCNT len = CTX_LEN(VAL_CONTEXT(context));

                    if (len > 0)
                        ASSERT_VALUE_MANAGED(ARR_HEAD(transcoded));

                    Bind_Values_All_Deep(
                        ARR_HEAD(transcoded),
                        VAL_CONTEXT(context)
                    );

                    REBVAL vali;
                    SET_INTEGER(&vali, len);

                    Resolve_Context(
                        VAL_CONTEXT(context),
                        Lib_Context,
                        &vali,
                        FALSE, // !all
                        FALSE // !expand
                    );
                }

                // Might think to use Append_Block here, but it's under
                // an #ifdef and apparently unused.  This is its definition.

                Insert_Series(
                    AS_SERIES(aggregate),
                    ARR_LEN(aggregate),
                    reinterpret_cast<REBYTE*>(ARR_HEAD(transcoded)),
                    ARR_LEN(transcoded)
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
            if (ANY_ARRAY(constructOutDatatypeIn)) {
                // They actually wanted a constructed value, and they wanted
                // effectively our aggregate...maybe with a different type.
                // Depending on how much was set in the "datatype in" we may
                // not have to rewrite the header bits (but Val_Inits do).

                Init_Any_Array(constructOutDatatypeIn, resultType, aggregate);

                // Val_Init makes aggregate a managed series, can't free it
                is_aggregate_managed = true;
            }
            else if (IS_OBJECT(constructOutDatatypeIn)) {
                // They want to create a "Context"; so we need to execute
                // the aggregate as per Make_Object.  At the moment
                // we don't have an interface to give a context a parent
                // from RenCpp

                // Once again, the REBVAL that is taken as "block" isn't a
                // block value, but the value pointer at the *head* of
                // the block.  :-/

                REBCTX * object = Make_Selfish_Context_Detect(
                    REB_OBJECT,
                    nullptr,
                    ARR_HEAD(aggregate),
                    nullptr
                );

                // This sets REB_OBJECT in the header, possibly redundantly
                Init_Object(constructOutDatatypeIn, object);
            }
            else {
                // If they didn't want a block, then they better want the type
                // of the first thing in the block.  And there better be
                // something in that block.

                REBCNT len = ARR_LEN(aggregate);

                if (len != 1) {
                    // Requested construct, but a singular item didn't come
                    // back (either 0 or more than 1 element in aggregate)
                    Init_Error(
                        extraOut,
                        ::Error(RE_MISC) // Make error code for this...
                    );
                    result = REN_CONSTRUCT_ERROR;
                    goto return_result;
                }
                else if (resultType != VAL_TYPE(ARR_HEAD(aggregate))) {
                    // Requested construct and value type was wrong
                    Init_Error(
                        extraOut,
                        ::Error(
                            RE_INVALID_ARG, // Make error code for this...
                            ARR_HEAD(aggregate)
                        )
                    );
                    result = REN_CONSTRUCT_ERROR;
                    goto return_result;
                }
                else {
                    *constructOutDatatypeIn = *KNOWN(ARR_HEAD(aggregate));
                }
            }
        }

        if (applyOut) {
            applying = true;

            if (!is_aggregate_managed) {
                //
                // DO and its bretheren are not currently specifically written
                // to not call Val_Init_Block or otherwise on the passed in
                // values (for instance, to put them into a backtrace).  So
                // they would manage the series if we did not do so here.
                // Review this implementation detail for GC performance...
                //
                MANAGE_ARRAY(aggregate);
                is_aggregate_managed = TRUE;
            }

            // Array now managed, protect from GC
            //
            PUSH_GUARD_ARRAY(aggregate);

            if (Generalized_Apply_Throws(
                applyOut,
                applicand,
                aggregate,
                SPECIFIED // the aggregate is all REBVALs, fully specified
            )) {
                CATCH_THROWN(extraOut, applyOut);
                result = REN_APPLY_THREW;
            }
            else
                result = REN_SUCCESS;

            DROP_GUARD_ARRAY(aggregate);
        }
        else
            result = REN_SUCCESS;

        // Pop our error trapping state, then unsave the aggregate (must be
        // done in this order)

    return_result:

        // We only free the series if we didn't manage it.  For simplicity
        // we control this with a boolean flag for now.
        //
        assert(is_aggregate_managed == IS_ARRAY_MANAGED(aggregate));
        if (!is_aggregate_managed)
            Free_Array(aggregate);

        DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

        return result;
    }


    RenResult FormAsUtf8(
        RebolEngineHandle engine,
        REBVAL const * value,
        unsigned char * buffer,
        size_t bufSize,
        size_t * numBytesOut
    ) {
        assert(engine.data == 1020);

        // First we mold with the "FORM" settings and get a STRING!
        // series out of that.

        REB_MOLD mo;
        CLEARS(&mo);
        Push_Mold(&mo);
        Mold_Value(&mo, const_cast<REBVAL *>(value), FALSE);

        REBSER * utf8_series = Pop_Molded_UTF8(&mo);

        // Okay that should be the UTF8 data.  Let's copy it into the buffer
        // the caller sent us.

        size_t len = SER_LEN(utf8_series);
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

        std::copy(
            SER_HEAD(REBYTE, utf8_series),
            SER_HEAD(REBYTE, utf8_series) + len,
            buffer
        );

        Free_Series(utf8_series);

        return result;
    }

    RenResult ShimHalt() {
        fail (VAL_CONTEXT(TASK_HALT_ERROR));
        DEAD_END;
    }

    void ShimInitThrown(REBVAL *out, REBVAL const *value, REBVAL const *name) {
        if (name)
            *out = *name;
        else
            SET_VOID(out);

        // !!! There is no way to throw an EXIT_FROM from Ren-C, at present,
        // other than by calling the natives that do it.
        //
        if (value)
            CONVERT_NAME_TO_THROWN(out, value);
        else
            CONVERT_NAME_TO_THROWN(out, VOID_CELL);
    }

    RenResult ShimFail(REBVAL const * error) {
        fail (VAL_CONTEXT(error));
        DEAD_END;
    }

    ~RebolHooks () {
        // The runtime may be shutdown already, so don't do anything here
        // using REBVALs or REBSERs.  Put that in engine shutdown.
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
    RenCell * out,
    RenEngineHandle engine,
    char const * name
) {
    return ren::internal::hooks.FindContext(out, engine, name);
}


RenResult RenConstructOrApply(
    RebolEngineHandle engine,
    RenCell const * context,
    RenCell const * valuePtr,
    RenCell * const * loadablesPtr,
    size_t numLoadables,
    size_t sizeofLoadable,
    RenCell * constructOutTypeIn,
    RenCell * applyOut,
    RenCell * extraOut
) {
    return ren::internal::hooks.ConstructOrApply(
        engine,
        AS_C_REBVAL(context),
        AS_C_REBVAL(valuePtr),
        reinterpret_cast<REBVAL * const *>(loadablesPtr),
        numLoadables,
        sizeofLoadable,
        AS_REBVAL(constructOutTypeIn),
        AS_REBVAL(applyOut),
        AS_REBVAL(extraOut)
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
        engine, AS_C_REBVAL(value), buffer, bufSize, lengthOut
    );
}


RenResult RenShimHalt() {
    return ren::internal::hooks.ShimHalt();
}


void RenShimInitThrown(
    RenCell *out,
    RenCell const *value,
    RenCell const *name
) {
    ren::internal::hooks.ShimInitThrown(
        AS_REBVAL(out), AS_C_REBVAL(value), AS_C_REBVAL(name)
    );
}


RenResult RenShimFail(RenCell const * error) {
    return ren::internal::hooks.ShimFail(AS_C_REBVAL(error));
}
