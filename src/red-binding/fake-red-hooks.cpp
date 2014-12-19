//
// This is a fake implementation of the hook API as implemented against the
// RedCell type.  It was used to do basic diagnostics and ensure that the
// reference counting and everything was being handled smoothly.
//
// It is very hacky, and just gives fake answers back.  (Such as every
// evaluation resulting in a string, just to test the reference counts.)
//

#ifndef LINKED_WITH_RED_AND_NOT_A_TEST

#ifndef NDEBUG
#include <unordered_set>
#include <unordered_map>
#endif
#include <cassert>

#include "redcpp/red.hpp"


namespace ren {

class FakeRedHooks {
#ifndef NDEBUG
    std::unordered_map<
        RedEngineHandle,
        std::unordered_set<int32_t>
    > nodes;
#endif

public:
    FakeRedHooks () {
    }

    int AllocEngine(RedEngineHandle * handleOut) {
        // make unique handle for each call, identity by pointer
        *handleOut = evilPointerToInt32Cast(new int(1020));
#ifndef NDEBUG
        assert(nodes.find(*handleOut) == nodes.end());
        nodes[*handleOut] = std::unordered_set<int32_t> {};
#endif
        return REN_SUCCESS;
    }

    int FreeEngine(RedEngineHandle handle) {
#ifndef NDEBUG
        auto it = nodes.find(handle);

        assert(it != nodes.end());

        std::unordered_set<int32_t> & leftovers = (*it).second;
        assert (leftovers.empty());

        nodes.erase(it);
#endif

        delete evilInt32ToPointerCast<int *>(handle);
        return 0;
    }

    int ConstructOrApply(
        RedEngineHandle handle,
        RedCell * valuePtr,
        RedCell * loadablesPtr,
        int32_t numLoadables,
        int32_t sizeofLoadable,
        RedCell * constructOutDatatypeIn,
        RedCell * applyOut
    ) {
        print("--->[FakeRed::ConstructOrApply]--->");
        print("RedEngineHandle is", handle);

        if (valuePtr) {
            print("Value is", Value(*reinterpret_cast<Value*>(valuePtr)));
        } else {
            print("valuePtr is nullptr");
        }

        print("There are", numLoadables, "loadable entries:");

        char * currentPtr = reinterpret_cast<char*>(loadablesPtr);
        for (int32_t index = 0; index < numLoadables; index++) {
            Value valueTemp (Value::Dont::Initialize);
            valueTemp.cell = reinterpret_cast<Value *>(currentPtr)->cell;
            valueTemp.refcountPtr = nullptr;
            valueTemp.engine = REN_ENGINE_HANDLE_INVALID;

            if (
                RedRuntime::getDatatypeID(valueTemp) != runtime.TYPE_ALIEN
            ) {
                print("LOADED:", valueTemp);
            } else {
                print(
                    "PENDING:",
                    evilInt32ToPointerCast<char*>(valueTemp.cell.data1)
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

            if (runtime.needsRefcount(*constructOutDatatypeIn)) {
                constructOutDatatypeIn->s.data3 = evilPointerToInt32Cast(
                    new int(304)
                );
#ifndef NDEBUG
                nodes[handle].insert(constructOutDatatypeIn->s.data3);
#endif
            } else {
                constructOutDatatypeIn->s.data3 = 0;
            }
        } else {
            print("No construction requested.");
        }

        if (!applyOut) {
            print("No apply requested.");
        } else {
            print("Apply requested.");

            // We will fabricate a result for any applies and return
            // a string.  This way we exercise the reference counting.
            applyOut->header = RedRuntime::TYPE_STRING;
            applyOut->data1 = 1;
            applyOut->s.data2 = 0;

            applyOut->s.data3 = evilPointerToInt32Cast(
                new int {304}
            );
#ifndef NDEBUG
            nodes[handle].insert(applyOut->s.data3);
#endif
        }

        print("<---[FakeRed::ConstructOrApply]<---");
        return REN_SUCCESS;
    }

    int ReleaseCells(
        RedEngineHandle handle,
        RedCell *cellsPtr,
        int32_t numCells,
        int32_t sizeofCellBlock
    ) {
        char * currentPtr = reinterpret_cast<char*>(cellsPtr);
        for (int32_t index = 0; index < numCells; index++) {
            Value valueTemp {Value::Dont::Initialize};
            valueTemp.cell = reinterpret_cast<Value *>(currentPtr)->cell;
            valueTemp.refcountPtr = nullptr;

            assert(valueTemp.needsRefcount());

#ifndef NDEBUG
            auto it = nodes.find(handle);
            assert(it != nodes.end());

            size_t numErased = (*it).second.erase(valueTemp.cell.s.data3);
            assert(numErased == 1);
#else
            UNUSED(handle);
#endif

            delete evilInt32ToPointerCast<int *>(valueTemp.cell.s.data3);

            currentPtr += sizeofCellBlock;
        }
        return REN_SUCCESS;
    }

    ~FakeRedHooks() {
        assert(nodes.empty());
    }
};

FakeRedHooks hooks;

} // end namespace ren


int RenAllocEngine(RedEngineHandle * handleOut) {
    return ren::hooks.AllocEngine(handleOut);
}

int RenFreeEngine(RedEngineHandle handle) {
    return ren::hooks.FreeEngine(handle);
}

int RenConstructOrApply(
    RedEngineHandle handle,
    RedCell * valuePtr,
    RedCell * loadablesPtr,
    int32_t numLoadables,
    int32_t sizeofLoadable,
    RedCell * constructOutWithDatatype,
    RedCell * applyOut
) {
    return ren::hooks.ConstructOrApply(
        handle,
        valuePtr,
        loadablesPtr,
        numLoadables,
        sizeofLoadable,
        constructOutWithDatatype,
        applyOut
    );

}

int RenReleaseCells(
    RedEngineHandle handle,
    RedCell *cellsPtr,
    int32_t numCells,
    int32_t sizeofCellBlock
) {
    return ren::hooks.ReleaseCells(
        handle, cellsPtr, numCells, sizeofCellBlock
    );
}

#endif
