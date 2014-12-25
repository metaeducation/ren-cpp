//
// This is a fake implementation of the hook API as implemented against the
// RedCell type.  It was used initially to do basic diagnostics, to test
// the reference counting and other general ideas about what would be
// "on the other side of the fence" from the C++ binding.
//
// However, its hacky beginings led to the idea of switching over and using
// Rebol as a test engine that would actually work.  Being more complete, and
// written in C and able to debug-step-into it--it was an easier first target
// than Red.
//
// So this file's destiny is the trash bin, and it's getting trashier as that
// time comes closer.  It no longer needs to check the client side because
// the client is already checked well enough by the Rebol implementation.
// It just needs to be enough code to test to make sure the binding can
// be built without a Rebol dependency when you use -DRUNTIME=red
//

#ifndef LINKED_WITH_RED_AND_NOT_A_TEST

#ifndef NDEBUG
#include <unordered_set>
#include <unordered_map>

#include <cstring>
#include <sstream>
#endif
#include <cassert>

#include "rencpp/red.hpp"


namespace ren {

namespace internal {

class FakeRedHooks {

public:
    FakeRedHooks () {
    }


    RenResult AllocEngine(RedEngineHandle * engineOut) {
        engineOut->data = 1020;
        return REN_SUCCESS;
    }


    RenResult FreeEngine(RedEngineHandle engine) {
        UNUSED(engine);
        return REN_SUCCESS;
    }


    RenResult AllocContext(
        RedEngineHandle engine,
        RedContextHandle * contextOut
    ) {
        UNUSED(engine);
        contextOut->pointer = nullptr;
        return REN_SUCCESS;
    }


    RenResult FreeContext(
        RedEngineHandle engine,
        RenContextHandle context
    ) {
        UNUSED(engine);
        UNUSED(context);
        return REN_SUCCESS;
    }


    RenResult FindContext(
        RedEngineHandle engine,
        char const * name,
        RedContextHandle * contextOut
    ) {
        UNUSED(engine);
        UNUSED(name);
        contextOut->pointer = nullptr;
        return REN_SUCCESS;
    }


    RenResult ConstructOrApply(
        RedEngineHandle engine,
        RedContextHandle context,
        RedCell const * applicand,
        RedCell loadables[],
        size_t numLoadables,
        size_t sizeofLoadable,
        RedCell * constructOutDatatypeIn,
        RedCell * applyOut
    ) {
        UNUSED(context);
        print("--->[FakeRed::ConstructOrApply]--->");

        if (applicand) {
            char buffer[256];
            size_t length;
            RenFormAsUtf8(engine, applicand, buffer, 256, &length);
            print("Applicand is", buffer);
        } else {
            print("applicand is nullptr");
        }

        print("There are", numLoadables, "loadable entries:");

        char * currentPtr = reinterpret_cast<char*>(loadables);
        for (size_t index = 0; index < numLoadables; index++) {
            auto & cell = *reinterpret_cast<RenCell *>(currentPtr);

            if (RedRuntime::getDatatypeID(cell) != runtime.TYPE_ALIEN) {
                char buffer[256];
                size_t length;
                RenFormAsUtf8(engine, &cell, buffer, 256, &length);
                print("LOADED:", buffer);
            }
            else {
                print(
                    "PENDING:",
                    evilInt32ToPointerCast<char*>(cell.data1)
                );
            }

            currentPtr += sizeofLoadable;
        }

        if (constructOutDatatypeIn) {
            print(
                "Construction requested",
                "for DatatypeID", "=", constructOutDatatypeIn->header
            );

            // Blatantly lie by just setting header bits to match ID,
            // the data will be garbage.
            constructOutDatatypeIn->data1 = 0;
            constructOutDatatypeIn->s.data2 = 0;
            constructOutDatatypeIn->s.data3 = 0;
        }
        else {
            print("No construction requested.");
        }

        if (!applyOut) {
            print("No apply requested.");
        }
        else {
            print("Apply requested.");

            applyOut->header = RedRuntime::TYPE_STRING;
            applyOut->data1 = 1;
            applyOut->s.data2 = 0;
            applyOut->s.data3 = 0;
        }

        print("<---[FakeRed::ConstructOrApply]<---");
        return REN_SUCCESS;
    }

    RenResult ReleaseCells(
        RedEngineHandle engine,
        RedCell cells[],
        size_t numCells,
        size_t sizeofCellBlock
    ) {
        UNUSED(engine);
        UNUSED(cells);
        UNUSED(numCells);
        UNUSED(sizeofCellBlock);
        return REN_SUCCESS;
    }

    RenResult FormAsUtf8(
        RedEngineHandle engine,
        RedCell const * value,
        char * buffer,
        size_t bufSize,
        size_t * lengthOut
    ) {
        UNUSED(engine);

        std::stringstream ss;
    #ifndef NDEBUG
        ss << "Formed(" << RedRuntime::getDatatypeID(value) << ")";
    #else
        ss << "Formed("
            << static_cast<int>(RedRuntime::getDatatypeID(value))
            << ")";
    #endif
        assert(bufSize > ss.str().length());
        std::strcpy(buffer, ss.str().c_str());
        *lengthOut = ss.str().length();

        return REN_SUCCESS;
    }

    ~FakeRedHooks() {
    }
};

FakeRedHooks hooks;

} // end namespace internal

} // end namespace ren


RenResult RenAllocEngine(RenEngineHandle * handleOut) {
    return ren::internal::hooks.AllocEngine(handleOut);
}


RenResult RenFreeEngine(RenEngineHandle handle) {
    return ren::internal::hooks.FreeEngine(handle);
}


RenResult RenAllocContext(
    RenEngineHandle engine,
    RenContextHandle * contextOut
) {
    return ren::internal::hooks.AllocContext(engine, contextOut);
}


RenResult RenFreeContext(RenEngineHandle engine, RenContextHandle context) {
    return ren::internal::hooks.FreeContext(engine, context);
}


RenResult RenFindContext(
    RenEngineHandle engine,
    char const * name,
    RenContextHandle * contextOut
) {
    return ren::internal::hooks.FindContext(engine, name, contextOut);
}


RenResult RenConstructOrApply(
    RenEngineHandle engine,
    RenContextHandle context,
    RenCell const * applicand,
    RenCell loadables[],
    size_t numLoadables,
    size_t sizeofLoadable,
    RenCell * constructOutDatatypeIn,
    RenCell * applyOut
) {
    return ren::internal::hooks.ConstructOrApply(
        engine,
        context,
        applicand,
        loadables,
        numLoadables,
        sizeofLoadable,
        constructOutDatatypeIn,
        applyOut
    );

}


RenResult RenReleaseCells(
    RenEngineHandle handle,
    RenCell cells[],
    size_t numCells,
    size_t sizeofCellBlock
) {
    return ren::internal::hooks.ReleaseCells(
        handle, cells, numCells, sizeofCellBlock
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
        engine,
        value,
        buffer,
        bufSize,
        lengthOut
    );
}

#endif
