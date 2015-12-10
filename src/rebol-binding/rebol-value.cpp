#include <array>
#include <stdexcept>
#include <vector>

#include "rencpp/value.hpp"
#include "rencpp/arrays.hpp"
#include "rencpp/context.hpp"

#include "rencpp/rebol.hpp" // ren::internal::nodes

#include "rebol-common.hpp"


namespace ren {

//
// COMPARISON
//

bool AnyValue::isEqualTo(AnyValue const & other) const {
    // acts like REBNATIVE(equalq)

    REBVAL cell_copy;
    cell_copy = *AS_C_REBVAL(&cell);
    REBVAL other_copy;
    other_copy = *AS_C_REBVAL(&other.cell);

    // !!! Modifies arguments to coerce them for testing!
    return Compare_Modify_Values(&cell_copy, &other_copy, 0);
}

bool AnyValue::isSameAs(AnyValue const & other) const {
    // acts like REBNATIVE(sameq)

    REBVAL cell_copy;
    cell_copy = *AS_C_REBVAL(&cell);
    REBVAL other_copy;
    other_copy = *AS_C_REBVAL(&other.cell);

    // !!! Modifies arguments to coerce them for testing
    return Compare_Modify_Values(&cell_copy, &other_copy, 3);
}



//
// The only way the client can get handles of types that need some kind of
// garbage collection participation right now is if the system gives it to
// them.  So on the C++ side, they never create series (for instance).  It's
// a good check of the C++ wrapper to do some reference counting of the
// series that have been handed back.
//

bool AnyValue::tryFinishInit(RenEngineHandle engine) {
    // Safe to test this before potentially put into a list...these had to be
    // initialized to nullptr instead of left as-is in order to be safe for
    // freeing in case the destructor got called when finishInit never did...
    assert(not next and not prev);

    // For the immediate moment, we have only one engine, but taking note
    // when that engine isn't being threaded through the values is a good
    // catch of problems for when there's more than one...
    assert(engine.data == 1020);
    origin = engine;

    // We shouldn't be able to get any REB_END values made in Ren/C++
    assert(NOT_END(AS_REBVAL(&cell)));

    // We no longer allow AnyValue to hold a REB_UNSET (unless specialization
    // using std::optional<AnyValue> represents unsets using that, which would
    // happen sometime later down the line when that optimization makes sense)
    // finishInit() is an inline wrapper that throws if this happens.
    if (IS_UNSET(AS_REBVAL(&cell)))
        return false;

    if (FLAGIT_64(VAL_TYPE(AS_REBVAL(&cell))) & TS_NO_GC) {
        // Types with no GC-aware members do not need to be put into the list
        // While the pointers are there anyway and it saves no memory, it does
        // save time when a GC runs...as well as the cost of any sync
        return true;
    }

    // !!! Placeholder for a less-global locking strategy (list per type, or
    // maybe hashed out somehow for less contention?)
    std::lock_guard<std::mutex> lock(internal::linkMutex);

    if (internal::head) {
        assert(this != internal::head);
        internal::head->prev = this;
        next = internal::head;
    } // else leave next null

    // prev is already null

    internal::head = this; // update head

    return true;
}


void AnyValue::uninitialize() {

    // !!! We could avoid taking the mutex if the type wasn't tracked, but we
    // do not pay for initialization of the cell bits.  So this check could
    // touch uninitialized memory in the destructor case:
    //
    //     FLAGIT_64(VAL_TYPE(&temp->cell)) & TS_NO_GC
    //
    // The cell bits would need to be initialized at least to REB_END or
    // similar.  Or some other way of marking the next and prev with magic
    // numbers to distinguish from the nullptr/nullptr case that may be
    // the sole linked in element in the chain.

    std::lock_guard<std::mutex> lock(internal::linkMutex);

    if (internal::head == this) {
        assert(not prev);
        internal::head = next;
    }
    else if (prev) // possible to be false if finishInit never called...
        prev->next = next;

    if (next) next->prev = prev;

    next = prev = nullptr;

    origin = REN_ENGINE_HANDLE_INVALID;
}


void AnyValue::toCell_(
    RenCell & cell, optional<AnyValue> const & value
) noexcept {
    if (value == nullopt)
        SET_UNSET(AS_REBVAL(&cell));
    else
        cell = value->cell;
}


AnyValue AnyValue::copy(bool deep) const {

    // It seems the only way to call an action is to put the arguments it
    // expects onto the stack :-/  For instance in the dispatch of A_COPY
    // we see it uses D_REF(ARG_COPY_DEEP) in the block handler to determine
    // whether to copy deeply.  So there is no deep flag to Copy_Value.  :-/
    // Exactly what the incantation would be can be figured out another
    // day but it would look something(?) like this commented out code...

  /*
    auto saved_DS_TOP = DS_TOP;

    AnyValue result (Dont::Initialize);

    DS_PUSH(&cell); // value
    DS_PUSH_NONE; // /part
    DS_PUSH_NONE; // length
    // /deep
    if (deep)
        DS_PUSH_TRUE;
    else
        DS_PUSH_NONE;
    DS_PUSH_NONE; // /types
    DS_PUSH_NONE; // kinds

    // for actions, the result is written to the same location as the input
    result.cell = cell;
    Do_Act(DS_TOP, VAL_TYPE(&cell), A_COPY);
    result.finishInit(origin);

    DS_TOP = saved_DS_TOP;
   */

    // However, that's not quite right and so the easier thing for the moment
    // is to just invoke it like running normal code.  :-/  But if anyone
    // feels like fixing the above, be my guest...

    Context userContext (Dont::Initialize);
    Val_Init_Object(
        AS_REBVAL(&userContext.cell),
        VAL_FRAME(Get_System(SYS_CONTEXTS, CTX_USER))
    );
    userContext.finishInit(origin);

    std::array<internal::Loadable, 2> loadables = {{
        deep ? "copy/deep" : "copy",
        *this
    }};

    AnyValue result (Dont::Initialize);

    constructOrApplyInitialize(
        origin,
        &userContext, // hope that COPY hasn't been overwritten...
        nullptr, // no applicand
        loadables.data(),
        loadables.size(),
        nullptr, // Don't construct
        &result // Do apply
    );

    return result;
}


//
// BASIC STRING CONVERSIONS
//

std::string to_string(AnyValue const & value) {
    const size_t defaultBufLen = 100;

    // Note .data() method is const on std::string.
    //    http://stackoverflow.com/questions/7518732/

    std::vector<REBYTE> buffer (defaultBufLen);

    size_t numBytes;


    switch (
        RenFormAsUtf8(
            value.origin, &value.cell, buffer.data(), defaultBufLen, &numBytes
        ))
    {
        case REN_SUCCESS:
            assert(numBytes <= defaultBufLen);
            break;

        case REN_BUFFER_TOO_SMALL: {
            assert(numBytes > defaultBufLen);
            buffer.resize(numBytes);

            size_t numBytesNew;
            if (
                RenFormAsUtf8(
                    value.origin,
                    &value.cell,
                    buffer.data(),
                    numBytes,
                    &numBytesNew
                ) != REN_SUCCESS
            ) {
                throw std::runtime_error("Expansion failure in RenFormAsUtf8");
            }
            assert(numBytesNew == numBytes);
            break;
        }

        default:
            throw std::runtime_error("Unknown error in RenFormAsUtf8");
    }

    auto result = std::string(cs_cast(buffer.data()), numBytes);
    return result;
}


#if REN_CLASSLIB_QT == 1

QString to_QString(AnyValue const & value) {
    const size_t defaultBufLen = 100;

    QByteArray buffer (defaultBufLen, Qt::Uninitialized);

    size_t numBytes;

    // Note .data() method is const on std::string.
    //    http://stackoverflow.com/questions/7518732/

    switch (
        RenFormAsUtf8(
            value.origin,
            &value.cell,
            b_cast(buffer.data()),
            defaultBufLen,
            &numBytes
        ))
    {
        case REN_SUCCESS:
            assert(numBytes <= defaultBufLen);
            break;

        case REN_BUFFER_TOO_SMALL: {
            assert(numBytes > defaultBufLen);
            buffer.resize(static_cast<int>(numBytes));

            size_t numBytesNew;
            if (
                RenFormAsUtf8(
                    value.origin,
                    &value.cell,
                    b_cast(buffer.data()),
                    numBytes,
                    &numBytesNew
                ) != REN_SUCCESS
            ) {
                throw std::runtime_error("Expansion failure in RenFormAsUtf8");
            }
            assert(numBytesNew == numBytes);
            break;
        }

        default:
            throw std::runtime_error("Unknown error in RenFormAsUtf8");
    }

    buffer.truncate(static_cast<int>(numBytes));
    auto result = QString {buffer};
    return result;
}

#endif


//
// "LOADABLE" VALUE(S) TYPE
//

namespace internal {

Loadable::Loadable (char const * sourceCstr) :
    AnyValue (AnyValue::Dont::Initialize)
{
    // Using REB_TRASH as our "alien"; it's not a legal value type to request
    // but unlike a REB_END it can still carry a full value-cell's payload.
    // Also, if the trash is not transformed into an actual value before it
    // gets to the Ren-C core then it will trigger alerts and asserts.
    //
    VAL_RESET_HEADER(AS_REBVAL(&cell), REB_TRASH);
    VAL_HANDLE_DATA(AS_REBVAL(&cell)) = const_cast<char *>(sourceCstr);

    next = nullptr;
    prev = nullptr;
    origin = REN_ENGINE_HANDLE_INVALID;
}


Loadable::Loadable (optional<AnyValue> const & value) :
    AnyValue (AnyValue::Dont::Initialize)
{
    if (value == nullopt)
        SET_UNSET(AS_REBVAL(&cell));
    else
        cell = value->cell;

    // We trust that the source value we copied from will stay alive and
    // prevent garbage collection... (review this idea)
    next = nullptr;
    prev = nullptr;
    origin = REN_ENGINE_HANDLE_INVALID;
}


} // end namespace internal

} // end namespace ren
