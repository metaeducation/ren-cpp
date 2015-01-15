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
#include "rencpp/helpers.hpp"

#undef RenCell
#undef RenEngineHandle

// Very temporary
#define UNUSED(x) static_cast<void>(x)

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


    RenResult FindContext(
        RedEngineHandle engine,
        char const * name,
        RedCell * contextOut
    ) {
        UNUSED(engine);
        UNUSED(name);
        UNUSED(contextOut);
        return REN_SUCCESS;
    }


    RenResult ConstructOrApply(
        RedEngineHandle engine,
        RedCell const * context,
        RedCell const * applicand,
        RedCell const * loadablesCell,
        size_t numLoadables,
        size_t sizeofLoadable,
        RedCell * constructOutDatatypeIn,
        RedCell * applyOut,
        RedCell * errorOut
    ) {
        UNUSED(context);
        UNUSED(errorOut);
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

        auto currentPtr = reinterpret_cast<char const *>(loadablesCell);
        for (size_t index = 0; index < numLoadables; index++) {
            auto & cell = *reinterpret_cast<RenCell const *>(currentPtr);

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
        RedCell const * valuesCell,
        size_t numValues,
        size_t sizeofValue
    ) {
        UNUSED(engine);
        UNUSED(valuesCell);
        UNUSED(numValues);
        UNUSED(sizeofValue);
        return REN_SUCCESS;
    }

    RenResult FormAsUtf8(
        RedEngineHandle engine,
        RedCell const * cell,
        char * buffer,
        size_t bufSize,
        size_t * lengthOut
    ) {
        UNUSED(engine);

        std::stringstream ss;
    #ifndef NDEBUG
        ss << "Formed(" << RedRuntime::datatypeName(RedRuntime::getDatatypeID(*cell)) << ")";
    #else
        ss << "Formed("
            << static_cast<int>(RedRuntime::getDatatypeID(cell))
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

#define RenCell RedCell
#define RenEngineHandle RedEngineHandle



RenResult RenFindContext(
    RenEngineHandle engine,
    char const * name,
    RenCell * contextOut
) {
    return ren::internal::hooks.FindContext(engine, name, contextOut);
}


RenResult RenConstructOrApply(
    RenEngineHandle engine,
    RenCell const * context,
    RenCell const * applicand,
    RenCell const * loadablesCell,
    size_t numLoadables,
    size_t sizeofLoadable,
    RenCell * constructOutDatatypeIn,
    RenCell * applyOut,
    RenCell * errorOut
) {
    return ren::internal::hooks.ConstructOrApply(
        engine,
        context,
        applicand,
        loadablesCell,
        numLoadables,
        sizeofLoadable,
        constructOutDatatypeIn,
        applyOut,
        errorOut
    );

}


RenResult RenReleaseCells(
    RenEngineHandle handle,
    RenCell const * valuesCell,
    size_t numValues,
    size_t sizeofValue
) {
    return ren::internal::hooks.ReleaseCells(
        handle, valuesCell, numValues, sizeofValue
    );
}


RenResult RenFormAsUtf8(
    RenEngineHandle engine,
    RenCell const * cell,
    char * buffer,
    size_t bufSize,
    size_t * lengthOut
) {
    return ren::internal::hooks.FormAsUtf8(
        engine,
        cell,
        buffer,
        bufSize,
        lengthOut
    );
}

#endif
