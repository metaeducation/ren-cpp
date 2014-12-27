#ifndef NDEBUG
#include <unordered_set>
#include <unordered_map>
#endif
#include <cassert>

#include "rencpp/rebol.hpp"

#include <vector>
#include <algorithm>

#include <thread>

extern "C" {
#include "rebol/src/include/sys-value.h"
#include "rebol/src/include/sys-state.h"

extern jmp_buf * Halt_State;
void Init_Task_Context();	// Special REBOL values per task

}


namespace ren {

namespace internal {

#ifndef DEBUG
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


///
/// THREAD INITIALIZATION
///

//
// There are some thread-local pieces of state that need to be initialized
// or Rebol will complain.  However, we also want to be able to initialize
// on demand from whichever thread calls first.  This means we need to
// keep a list of which threads have been initialized and which not.
//
// REVIEW: what if threads die and don't tell us?  Is there some part of
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
            int marker;
            REBCNT bounds = OS_CONFIG(1, 0);

            if (bounds == 0)
                bounds = STACK_BOUNDS;

        #ifdef OS_STACK_GROWS_UP
            Stack_Limit = (REBCNT)(&marker) + bounds;
        #else
            if (bounds > (REBCNT)(&marker))
                Stack_Limit = 100;
            else
                Stack_Limit = (REBCNT)(&marker) - bounds;
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
                    throw exit_command (VAL_INT32(VAL_ERR_VALUE(DS_NEXT)));
                }
                if (VAL_ERR_NUM(val) == RE_HALT) {
                    throw evaluation_cancelled {};
                }
                throw evaluation_error (Value (engine, *val));
            }
            SET_STATE(state, Halt_State);

            // Use this handler for both, halt conditions (QUIT, HALT) and error
            // conditions. As this is a top-level handler, simply overwriting
            // Saved_State is safe.
            Saved_State = Halt_State;

            Init_Task();	// Special REBOL values per task

            // Pop our error trapping state

            POP_STATE(state, Halt_State);
            Saved_State = Halt_State;

            threadsSeen.push_back(std::this_thread::get_id());
        }
    }

///
/// ENGINE ALLOCATION AND FREEING
///

    int AllocEngine(RebolEngineHandle * engineOut) {
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


    int FreeEngine(RebolEngineHandle engine) {

        assert(engine.data == 1020);
        theEngine = REBOL_ENGINE_HANDLE_INVALID;

        return REN_SUCCESS;
    }



///
/// CONTEXT ALLOCATION AND FREEING
///

    int AllocContext(
        RebolEngineHandle engine,
        RebolContextHandle * contextOut
    ) {
        lazyThreadInitializeIfNeeded(engine);

        assert(not REBOL_IS_ENGINE_HANDLE_INVALID(theEngine));
        assert(engine.data == theEngine.data);

        if (not allocatedContexts) {
            allocatedContexts = Make_Block(10);

            // Protect from GC (thus protecting contents)
            // It's not entirely clear if this is implemented.
            /* SERIES_SET_FLAG(allocatedContexts, SER_EXT);*/
            SAVE_SERIES(allocatedContexts);
        }

        REBVAL block;
        Set_Block(&block, Make_Block(10));
        REBVAL * object = Append_Value(allocatedContexts);
        Set_Object(object, Make_Object(nullptr, &block));

        contextOut->series = VAL_OBJ_FRAME(object);

    #ifndef NDEBUG
        assert(nodes.find(engine.data) == nodes.end());
        nodes[engine.data]
            = std::unordered_map<REBSER const *, unsigned int> {};
    #endif
        return REN_SUCCESS;
    }


    int FreeContext(
        RebolEngineHandle engine,
        RebolContextHandle context
    ) {
        lazyThreadInitializeIfNeeded(engine);

        assert(allocatedContexts);

        bool removed = false;
        for (REBCNT index = 0; index < BLK_LEN(allocatedContexts); index++) {
            assert(IS_OBJECT(BLK_SKIP(allocatedContexts, index)));
            if (
                reinterpret_cast<REBSER *>(context.series)
                == VAL_OBJ_FRAME(BLK_SKIP(allocatedContexts, index))
            ) {
                Remove_Series(allocatedContexts, index, 1);
                removed = true;
                break;
            }
        }

        if (not removed)
            throw std::runtime_error("Couldn't find context in FreeContext");

        if (BLK_LEN(allocatedContexts) == 0) {
            // Allow GC
            /*SERIES_CLR_FLAG(allocatedContexts, SER_EXT); */
            UNSAVE_SERIES(allocatedContexts);
        }

    #ifndef NDEBUG
        auto it = nodes.find(engine.data);

        assert(it != nodes.end());

        std::unordered_map<REBSER const *, unsigned int> & leftovers
            = (*it).second;
        assert (leftovers.empty());

        nodes.erase(it);
    #endif

        return REN_SUCCESS;
    }


    int FindContext(
        RebolEngineHandle engine,
        char const * name,
        RebolContextHandle * contextOut
    ) {
        lazyThreadInitializeIfNeeded(engine);

        assert(not REBOL_IS_ENGINE_HANDLE_INVALID(theEngine));
        assert(engine.data == theEngine.data);

        REBSER * ctx = nullptr;
        if (strcmp(name, "USER") == 0)
            ctx = VAL_OBJ_FRAME(Get_System(SYS_CONTEXTS, CTX_USER));
        else if (strcmp(name, "LIB") == 0)
            ctx = VAL_OBJ_FRAME(Get_System(SYS_CONTEXTS, CTX_LIB));
        if (strcmp(name, "SYS") == 0)
            ctx = VAL_OBJ_FRAME(Get_System(SYS_CONTEXTS, CTX_SYS));

        // don't expose CTX_ROOT, I assume?

        if (ctx) {
            contextOut->series = ctx;
            return REN_SUCCESS;
        } else {
            *contextOut = REBOL_CONTEXT_HANDLE_INVALID;
            return REN_ERROR_NO_SUCH_CONTEXT;
        }
    }



///
/// GENERALIZED APPLY
///

    //
    // "Generalized Apply" is at the heart of the working of the binding, yet
    // what we currently call APPLY only works for function values.  This
    // is a "cheating" implementation and defers to APPLY only in the case
    // where the applicand is a function.
    //

    int Generalized_Apply(REBVAL * applicand, REBSER * args, REBFLG reduce) {

        if (IS_FUNCTION(applicand)) {

            // We get args as a series, but Apply_Block expects a RebVal.
            REBVAL block;
            VAL_SET(&block, REB_BLOCK);
            Set_Block(&block, args);

            Apply_Block(applicand, &block, reduce);

        }
        else if (IS_ERROR(applicand)) {
            if (SERIES_LEN(args) - 1 != 0)
                return REN_ERROR_TOO_MANY_ARGS;

            // from REBNATIVE(do) ?  What's the difference?  How return?
            if (IS_THROW(applicand)) {
                DS_PUSH(applicand);
            } else {
                Throw_Error(VAL_ERR_OBJECT(applicand));

                throw std::runtime_error(
                    "Threw error from Generalized_Apply but it kept running"
                );
            }
        }
        else {
            assert(not reduce); // To be added?

            Insert_Series(args, 0, reinterpret_cast<REBYTE *>(applicand), 1);

            REBCNT index = Do_Next(args, 0, FALSE /* not op! */);
            if (index != SERIES_TAIL(args)) {
                return REN_ERROR_TOO_MANY_ARGS;
            }

            Remove_Series(args, 0, 1);
        }

        // Result on top of stack
        return REN_SUCCESS;
    }


///
/// CONSTRUCT OR APPLY HOOK
///

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

    int ConstructOrApply(
        RebolEngineHandle engine,
        RebolContextHandle context,
        REBVAL const * valuePtr,
        REBVAL * loadablesPtr,
        size_t numLoadables,
        size_t sizeofLoadable,
        REBVAL * constructOutDatatypeIn,
        REBVAL * applyOut
    ) {
        UNUSED(engine);

        lazyThreadInitializeIfNeeded(engine);

        REBOL_STATE state;

        // Haphazardly copied from c-do.c and Do_String; review needed now
        // that it is understood better.
        //
        //     https://github.com/hostilefork/rencpp/issues/21

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
                throw exit_command (VAL_INT32(VAL_ERR_VALUE(DS_NEXT)));
            }
            if (VAL_ERR_NUM(val) == RE_HALT) {
                throw evaluation_cancelled {};
            }
            throw evaluation_error (Value (engine, *val));
        }
        SET_STATE(state, Halt_State);

        // Use this handler for both, halt conditions (QUIT, HALT) and error
        // conditions. As this is a top-level handler, simply overwriting
        // Saved_State is safe.
        Saved_State = Halt_State;

        int result = REN_SUCCESS;

        if (valuePtr) {
            // This is the current rule and the code expects it to be true,
            // but if it were not what might it mean?  This would be giving
            // a value but not asking for a result.  It's free to return
            // the result so this would never be done for performance.
            assert(applyOut);
        }

        char * current = reinterpret_cast<char *>(loadablesPtr);

        // Vector is bad to use with setjmp/longjmp, fix this

        std::vector<std::pair<REBSER *, REBVAL *>> tempSeriesOrValues;

        // For the initial state of the binding we'll focus on correctness
        // instead of optimization.  That means we'll take the "loadables"
        // and form a block out of them--even when we weren't asked to,
        // rather than implement a more efficient form of enumeration.

        // If we were asked to construct a block type, then this will be
        // the block we return...as there was no block indicator in the
        // initial string.  If we were asking to construct a non-block type,
        // then it should be the first element in this block.

        REBSER * aggregate = Make_Block(static_cast<REBCNT>(numLoadables * 2));
        SAVE_SERIES(aggregate);

        for (size_t index = 0; index < numLoadables; index++) {
            REBVAL * loadable = reinterpret_cast<REBVAL *>(current);
            if (VAL_TYPE(loadable) == REB_END) {

                // This is our "Alien" type that wants to get loaded.  Key
                // to his loading problem is that he wants to know whether
                // he is an explicit or implicit block type.  So that means
                // discerning between "foo bar" and "[foo bar]", which we
                // get through transcode which returns [foo bar] and
                // [[foo bar]] that discern the cases

                auto loadText = reinterpret_cast<REBYTE*>(
                    loadable->data.integer
                );

                REBSER * transcoded = Scan_Source(
                    loadText, LEN_BYTES(loadText)
                );

                // This is what Do_String did by default...except it only
                // worked with the user context.  Fell through to lib.

                REBCNT len = context.series->tail;

                Bind_Block(
                    context.series, BLK_HEAD(transcoded), BIND_ALL | BIND_DEEP
                );

                REBVAL vali;
                SET_INTEGER(&vali, len);

                Resolve_Context(context.series, Lib_Context, &vali, FALSE, 0);

                // Might think to use Append_Block here, but it's under
                // an #ifdef and apparently unused.  This is its definition.

                Insert_Series(
                    aggregate,
                    aggregate->tail,
                    reinterpret_cast<REBYTE*>(BLK_HEAD(transcoded)),
                    transcoded->tail
                );
            } else {
                // Just an ordinary value
                Append_Val(aggregate, loadable);
            }

            current += sizeofLoadable;
        }

        if (applyOut) {
            if (valuePtr) {
                result = Generalized_Apply(
                    const_cast<REBVAL *>(valuePtr),
                    const_cast<REBSER *>(aggregate),
                    FALSE
                );
                // even if there was an error, we need to keep going and
                // safely clean things up.
                if (result == REN_SUCCESS)
                    *applyOut = *DS_TOP;
                else
                    SET_UNSET(applyOut);
            } else {
                // Assume that nullptr for applicand means "just do the block
                // that was in the loadables".  This keeps us from having to
                // export a version of DO separately.

                *applyOut = *Do_Blk(aggregate, 0); // result is volatile
            }
        }

        if (constructOutDatatypeIn) {
            REBOL_Types resultType = static_cast<REBOL_Types>(
                VAL_TYPE(constructOutDatatypeIn)
            );
            if (ANY_BLOCK(constructOutDatatypeIn)) {
                // They actually wanted a constructed value, and they wanted
                // effectively our aggregate...maybe with a different type.
                Set_Block(constructOutDatatypeIn, aggregate);
            } else {
                // If they didn't want a block, then they better want the type
                // of the first thing in the block.  And there better be
                // something in that block.

                REBCNT len = BLK_LEN(aggregate);

                if (len == 0)
                    throw std::runtime_error(
                        "Requested construct and no value"
                    );

                if (len > 1)
                    throw std::runtime_error(
                        "Requested construct and more than one value"
                    );

                *constructOutDatatypeIn = *BLK_HEAD(aggregate);

                if (resultType != VAL_TYPE(constructOutDatatypeIn))
                    throw std::runtime_error(
                        "Requested construct and value type was wrong"
                    );
            }
        }

        // Same for our aggregate, though we may have returned it.  If it's
        // going to be defended from the garbage collector, we need hooks to
        // be written taking the binding refs into account

        UNSAVE_SERIES(aggregate);

        // Pop our error trapping state

        POP_STATE(state, Halt_State);
        Saved_State = Halt_State;

        return result;
    }

    int ReleaseCells(
        RebolEngineHandle engine,
        REBVAL * cellsPtr,
        size_t numCells,
        size_t sizeofCellBlock
    ) {
        lazyThreadInitializeIfNeeded(engine);

        char * current = reinterpret_cast<char *>(cellsPtr);
        for (size_t index = 0; index < numCells; index++) {
            REBVAL * cell = reinterpret_cast<REBVAL *>(current);

        #ifndef NDEBUG
            assert(ANY_SERIES(cell));
            auto it = nodes[engine.data].find(VAL_SERIES(cell));
            assert(it != nodes[engine.data].end());

            (*it).second--;
            if ((*it).second == 0) {
                size_t numErased = nodes[engine.data].erase(VAL_SERIES(cell));
                assert(numErased == 1);
                if (nodes[engine.data].empty())
                    nodes.erase(engine.data);
            }
        #else
            UNUSED(engine);
        #endif

            current += sizeofCellBlock;
        }

        return REN_SUCCESS;
    }


    RenResult FormAsUtf8(
        RebolEngineHandle engine,
        REBVAL const * value,
        char * buffer,
        size_t bufSize,
        size_t * numBytesOut
    ) {
        lazyThreadInitializeIfNeeded(engine);

#ifndef NDEBUG
        if (ANY_SERIES(value)) {
            auto it = nodes.find(engine.data);
            assert(it != nodes.end());
            assert(
                (*it).second.find(reinterpret_cast<REBSER const *>(
                    VAL_SERIES(value))
                )
                != (*it).second.end()
            );
        }
#endif

        REB_MOLD mo;
        mo.series = nullptr;
        mo.opts = 0;
        mo.indent = 0;
        mo.period = 0;
        mo.dash = 0;
        mo.digits = 0;

        Reset_Mold(&mo);
        Mold_Value(&mo, const_cast<REBVAL *>(value), 0);

        // w_char internally.  Look into actually doing proper UTF8 encoding
        // with binary.

        REBCNT len = SERIES_LEN(mo.series) - 1;
        *numBytesOut = static_cast<size_t>(len);

        RenResult result;
        if (len > bufSize) {
            len = bufSize;
            result = REN_BUFFER_TOO_SMALL;
        }
        else {
            result = REN_SUCCESS;
        }

        for (REBCNT index = 0; index < len; index++) {
            buffer[index] = reinterpret_cast<char*>(SERIES_DATA(mo.series))[index * 2];
        }

        return result;
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


RenResult RenAllocContext(
    RebolEngineHandle engine,
    RebolContextHandle * contextOut
) {
    return ren::internal::hooks.AllocContext(engine, contextOut);
}


RenResult RenFreeContext(RenEngineHandle engine, RenContextHandle context) {
    return ren::internal::hooks.FreeContext(engine, context);
}


RenResult RenFindContext(
    RenEngineHandle engine,
    char const * name,
    RenContextHandle *contextOut
) {
    return ren::internal::hooks.FindContext(engine, name, contextOut);
}


RenResult RenConstructOrApply(
    RebolEngineHandle engine,
    RebolContextHandle context,
    REBVAL const * valuePtr,
    REBVAL * loadablesPtr,
    size_t numLoadables,
    size_t sizeofLoadable,
    REBVAL * constructOutWithDatatype,
    REBVAL * applyOut
) {
    return ren::internal::hooks.ConstructOrApply(
        engine,
        context,
        valuePtr,
        loadablesPtr,
        numLoadables,
        sizeofLoadable,
        constructOutWithDatatype,
        applyOut
    );

}


RenResult RenReleaseCells(
    RebolEngineHandle engine,
    REBVAL *cellsPtr,
    size_t numCells,
    size_t sizeofCellBlock
) {
    return ren::internal::hooks.ReleaseCells(
        engine, cellsPtr, numCells, sizeofCellBlock
    );
}


RenResult RenFormAsUtf8(
    RenEngineHandle engine,
    RenCell const * value,
    char * buffer,
    size_t bufSize,
    size_t * lengthOut
) {
    return ren::internal::hooks.FormAsUtf8(
        engine, value, buffer, bufSize, lengthOut
    );
}
