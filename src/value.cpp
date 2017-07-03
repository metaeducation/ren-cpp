//
// value.cpp
// This file is part of RenCpp
// Copyright (C) 2015-2017 HostileFork.com
//
// Licensed under the Boost License, Version 1.0 (the "License")
//
//      http://www.boost.org/LICENSE_1_0.txt
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied.  See the License for the specific language governing
// permissions and limitations under the License.
//
// See http://rencpp.hostilefork.com for more information on this project
//

#include <ostream>
#include <vector>
#include <iostream>
#include <array>
#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/atoms.hpp"
#include "rencpp/context.hpp"
#include "rencpp/runtime.hpp"
#include "rencpp/error.hpp"
#include "rencpp/strings.hpp"

#include "rencpp/rebol.hpp" // ren::internal::nodes

#include "common.hpp"


namespace ren {



AnyValue::operator bool() const {
    return !(hasType<Blank>(*this) || isFalse());
}


//
// GENERALIZED APPLY
//

optional<AnyValue> AnyValue::apply_(
    internal::Loadable const loadables[],
    size_t numLoadables,
    AnyContext const * contextPtr,
    Engine * engine
) const {
    AnyValue result (Dont::Initialize);

    AnyContext context = contextPtr
        ? *contextPtr
        : AnyContext::current(engine);

    if (constructOrApplyInitialize(
        context.getEngine(),
        &context,
        this, // no applicand
        loadables,
        numLoadables,
        nullptr, // don't construct
        &result // do apply
    )) {
        return result;
    }

    return nullopt;
}


optional<AnyValue> AnyValue::apply(
    std::initializer_list<internal::Loadable> loadables,
    internal::ContextWrapper const & wrapper
) const {
    // This one has to be in the implementation file because it appears in
    // AnyValue, using Loadable, which is derived from AnyValue...
    return apply_(
        loadables.begin(),
        loadables.size(),
        &wrapper.context,
        nullptr
    );
}


optional<AnyValue> AnyValue::apply(
    std::initializer_list<internal::Loadable> loadables,
    Engine * engine
) const {
    // This one has to be in the implementation file because it appears in
    // AnyValue, using Loadable, which is derived from AnyValue...
    return apply_(loadables.begin(), loadables.size(), nullptr, engine);
}


bool AnyValue::constructOrApplyInitialize(
    RenEngineHandle engine,
    AnyContext const * context,
    AnyValue const * applicand,
    internal::Loadable const loadables[],
    size_t numLoadables,
    AnyValue * constructOutTypeIn,
    AnyValue * applyOut
) {
    AnyValue extraOut {AnyValue::Dont::Initialize};

    auto result = ::RenConstructOrApply(
        engine,
        context ? context->cell : nullptr,
        applicand ? applicand->cell : nullptr,
        numLoadables != 0 ? &loadables[0].cell : nullptr,
        numLoadables,
        sizeof(internal::Loadable),
        constructOutTypeIn ? constructOutTypeIn->cell : nullptr,
        applyOut ? applyOut->cell : nullptr,
        extraOut.cell
    );

    switch (result) {
        case REN_SUCCESS:
            break;

        case REN_CONSTRUCT_ERROR:
            extraOut.finishInit(engine);
            assert(hasType<Error>(extraOut));
            throw load_error {static_cast<Error>(extraOut)};

        case REN_APPLY_ERROR: {
            extraOut->finishInit(engine);
            assert(hasType<Error>(extraOut));
            throw evaluation_error {static_cast<Error>(extraOut)};
        }

        case REN_EVALUATION_HALTED:
            throw evaluation_halt {};

        case REN_APPLY_THREW: {
            bool hasName = applyOut->tryFinishInit(engine);
            bool hasValue = extraOut->tryFinishInit(engine);
            throw evaluation_throw {
                hasValue ? optional<AnyValue>{extraOut} : nullopt,
                hasName ? optional<AnyValue>{*applyOut} : nullopt
            };
        }

        default:
            throw std::runtime_error("Unknown error in RenConstructOrApply");
    }

    // It used to be required that we finalize the values before throwing
    // errors because (for instance) the tracking could be initialized.
    // That had to be changed because a Dont::Initialize was could construct
    // a type that could not survive an exception being thrown.  So we will
    // keep this finalization here just in case, because it should be safe now
    // to skip it in the case of an exception.

    if (constructOutTypeIn)
        constructOutTypeIn->finishInit(engine);

    if (applyOut) {
        // `tryFinishInit()` will give back false if the cell was not a value
        // (e.g. an "unset") which cues a caller requesting a value that they
        // should make a `nullopt` for the `optional<AnyValue>` instead of
        // considering the bits "good".
        return applyOut->tryFinishInit(engine);
    }

    // No apply requested, so same as not set
    return false;
}



//
// LOADABLE
//

internal::Loadable::Loadable (std::string const & source) :
    Loadable (Dont::Initialize)
{
    String value {source};
    *cell = *value.cell;
}

#if REN_CLASSLIB_QT == 1
internal::Loadable::Loadable (QString const & source) :
    Loadable (Dont::Initialize)
{
    String value {source};
    *cell = *value.cell;
}
#endif


// Even if asked not to initialize, we can't leave the type in a state where
// it cannot be safely freed.  Bad traversal pointers combined with bad data
// would be a problem.  Review this issue.

AnyValue::AnyValue (Dont)
{
    runtime.lazyInitializeIfNecessary();

    // We make a pairing of values, where the key stores extra tracking info.
    // The value is the cell we are interested in (what is returned from
    // make pairing).  We do not mark it managed, but rather manually free
    // it in the destructor, using C++ exception handling to take care of
    // error cases.
    //
    cell = reinterpret_cast<RenCell*>(Alloc_Pairing(NULL));

    REBVAL *key = PAIRING_KEY(AS_REBVAL(cell));
    Init_Blank(key);
    Init_Blank(AS_REBVAL(cell));

    // Mark the created pairing so it will act as a "root".  The key and value
    // will be deep marked for GC.
    //
    SET_VAL_FLAG(key, NODE_FLAG_ROOT);
}


//
// COMPARISON
//

bool AnyValue::isEqualTo(AnyValue const & other) const {
    // acts like REBNATIVE(equalq)

    DECLARE_LOCAL (cell_copy);
    Move_Value(cell_copy, AS_C_REBVAL(cell));
    DECLARE_LOCAL (other_copy);
    Move_Value(other_copy, AS_C_REBVAL(other.cell));

    // !!! Modifies arguments to coerce them for testing!
    return Compare_Modify_Values(cell_copy, other_copy, 0);
}

bool AnyValue::isSameAs(AnyValue const & other) const {
    // acts like REBNATIVE(sameq)

    DECLARE_LOCAL (cell_copy);
    Move_Value(cell_copy, AS_C_REBVAL(cell));
    DECLARE_LOCAL (other_copy);
    Move_Value(other_copy, AS_C_REBVAL(other.cell));

    // !!! Modifies arguments to coerce them for testing
    return Compare_Modify_Values(cell_copy, other_copy, 3);
}



//
// The only way the client can get handles of types that need some kind of
// garbage collection participation right now is if the system gives it to
// them.  So on the C++ side, they never create series (for instance).  It's
// a good check of the C++ wrapper to do some reference counting of the
// series that have been handed back.
//

bool AnyValue::tryFinishInit(RenEngineHandle engine) {

    // For the immediate moment, we have only one engine, but taking note
    // when that engine isn't being threaded through the values is a good
    // catch of problems for when there's more than one...
    assert(engine.data == 1020);
    origin = engine;

    // We shouldn't be able to get any REB_END values made in Ren/C++
    assert(NOT_END(AS_REBVAL(cell)));

    // We no longer allow AnyValue to hold a void (unless specialization
    // using std::optional<AnyValue> represents unsets using that, which would
    // happen sometime later down the line when that optimization makes sense)
    // finishInit() is an inline wrapper that throws if this happens.
    //
    if (IS_VOID(AS_REBVAL(cell)))
        return false;

    return true;
}


void AnyValue::uninitialize() {

    Free_Pairing(AS_REBVAL(cell));

    // drop refcount here

    origin = REN_ENGINE_HANDLE_INVALID;
}


void AnyValue::toCell_(
    RenCell * cell, optional<AnyValue> const & value
) noexcept {
    if (value == nullopt)
        Init_Void(AS_REBVAL(cell));
    else
        *cell = *value->cell;
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

    AnyContext userContext (Dont::Initialize);
    Init_Object(
        AS_REBVAL(userContext.cell),
        VAL_CONTEXT(Get_System(SYS_CONTEXTS, CTX_USER))
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
            value.origin, value.cell, buffer.data(), defaultBufLen, &numBytes
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
                    value.cell,
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

    // Currently, PUSH_UNHALTABLE_TRAP sets up the stack limit.  Anything that
    // calls C_STACK_OVERFLOWING(), e.g. MOLD, must have the Stack_Limit set
    // correctly for the running thread.

    REBCTX *error;
    struct Reb_State state;

    PUSH_UNHALTABLE_TRAP(&error, &state);

// The first time through the following code 'error' will be NULL, but...
// `fail` can longjmp here, so 'error' won't be NULL *if* that happens!

    if (error != NULL)
        throw std::runtime_error("Error during to_QString (stack overflow?)");

    const size_t defaultBufLen = 100;

    QByteArray buffer (defaultBufLen, Qt::Uninitialized);

    size_t numBytes;

    // Note .data() method is const on std::string.
    //    http://stackoverflow.com/questions/7518732/

    switch (
        RenFormAsUtf8(
            value.origin,
            value.cell,
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
                    value.cell,
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

    DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

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
    // Using REB_0 as our "alien"; it's not a legal value type to request
    // to be put into a block.
    //
    VAL_RESET_HEADER(AS_REBVAL(cell), REB_0);
    AS_REBVAL(cell)->payload.handle.data.pointer
        = const_cast<char *>(sourceCstr);

    origin = REN_ENGINE_HANDLE_INVALID;
}


Loadable::Loadable (optional<AnyValue> const & value) :
    AnyValue (AnyValue::Dont::Initialize)
{
    if (value == nullopt)
        Init_Void(AS_REBVAL(cell));
    else
        *cell = *value->cell;

    // We trust that the source value we copied from will stay alive and
    // prevent garbage collection... (review this idea)

    origin = REN_ENGINE_HANDLE_INVALID;
}


} // end namespace internal

} // end namespace ren
