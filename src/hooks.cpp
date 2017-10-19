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

#include "common.hpp"


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
        if (!REBOL_IS_ENGINE_HANDLE_INVALID(theEngine))
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
        REBVAL *out,
        RebolEngineHandle engine,
        char const * name
    ) {
        assert(!REBOL_IS_ENGINE_HANDLE_INVALID(theEngine));
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
            Init_Object(out, context);
            return REN_SUCCESS;
        }

        return REN_ERROR_NO_SUCH_CONTEXT;
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

        DECLARE_MOLD (mo);

        Push_Mold(mo);
        Form_Value(mo, const_cast<REBVAL *>(value));

        REBSER * utf8_series = Pop_Molded_UTF8(mo);

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

        // Used to use std::copy, but MSVC complains unless you use their
        // non-standard checked_array_iterator<>:
        //
        // https://stackoverflow.com/q/25716841/
        //
        memcpy(buffer, SER_HEAD(REBYTE, utf8_series), len);

        Free_Series(utf8_series);

        return result;
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
    REBVAL *out,
    RenEngineHandle engine,
    char const * name
) {
    return ren::internal::hooks.FindContext(out, engine, name);
}

RenResult RenFormAsUtf8(
    RenEngineHandle engine,
    REBVAL const * value,
    unsigned char * buffer,
    size_t bufSize,
    size_t * lengthOut
) {
    return ren::internal::hooks.FormAsUtf8(
        engine, value, buffer, bufSize, lengthOut
    );
}

REBVAL *RL_Arg(void *frame, int index)
{
    return FRM_ARG(reinterpret_cast<struct Reb_Frame *>(frame), index);
}

void RL_Move(REBVAL *out, REBVAL const * v)
{
    Move_Value(out, v);
}