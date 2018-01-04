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
    return isTruthy();
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


//
// The concept of ConstructOrApply was to make one primitive that could LOAD,
// splice blocks, evaluate without making a block out of the result, etc.
// One idea would be that if small string fragments were included as part of
// the code to execute, the transcoded arrays would not need to be persisted
// after the call was finished.
//
// Since it was first conceived, two years of development on the Ren-C
// evaluator have opened up new possibilities.
//
// The two main tricks at work are that it accepts a pointer to an array of
// values which may use REB_0 as a special holder for UTF-8 C strings to be
// spliced into the execution after they are loaded.  Note that if you want
// to actually get back the constructed value, you must pass in a constructOut
// which has the datatype field already set in the header that you want.
//
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

    assert(engine.data == 1020);

    // longjmp could "clobber" this variable if it were not volatile, and
    // code inside of the `if (error)` depends on possible modification
    // between the setjmp (PUSH_UNHALTABLE_TRAP) and the longjmp
    //
    volatile bool applying = false;

    struct Reb_State state;
    REBCTX * error;

    PUSH_UNHALTABLE_TRAP(&error, &state);

// The first time through the following code 'error' will be NULL, but...
// `fail` can longjmp here, 'error' won't be NULL *if* that happens!

    RenResult result;

    if (error) {
        // do not need to free series... it is done automatically

        if (ERR_NUM(error) == RE_HALT) {
            //
            // cancellation in middle of interpretation from outside
            // the evaluation loop (e.g. Escape).
            //
            throw evaluation_halt {};
        }

        Init_Error(extraOut.cell, error);
        
        if (applying) {
            extraOut->finishInit(engine);
            assert(hasType<Error>(extraOut));
            throw evaluation_error {static_cast<Error>(extraOut)};
        }

        extraOut.finishInit(engine);
        assert(hasType<Error>(extraOut));
        throw load_error {static_cast<Error>(extraOut)};
    }

    // Note: No C++ allocations can happen between here and the POP_STATE
    // calls as long as the C stack is in control, as setjmp/longjmp will
    // subvert stack unwinding and just reset the processor state.

    REBOOL is_aggregate_managed = FALSE;
    REBARR * aggregate = Make_Array(numLoadables * 2);

    if (applicand) {
        // This is the current rule and the code expects it to be true,
        // but if it were not what might it mean?  This would be giving
        // a value but not asking for a result.  It's free to return
        // the result so this would never be done for performance.
        assert(applyOut);
    }

    // We don't necessarily have a pointer to an array of REBVALs if
    // sizeof loadable != sizeof(REBVAL); so keep "current" as char*
    //
    auto current = reinterpret_cast<char const *>(
        numLoadables != 0 ? &loadables[0].cell : nullptr
    );

    // For the initial state of the binding we'll focus on correctness
    // instead of optimization.  That means we'll take the "loadables"
    // and form a block out of them--even when we weren't asked to,
    // rather than implement a more efficient form of enumeration.

    // If we were asked to construct a block type, then this will be
    // the block we return...as there was no block indicator in the
    // initial string.  If we were asking to construct a non-block type,
    // then it should be the first element in this block.

    for (size_t index = 0; index < numLoadables; index++) {

        auto cell = *reinterpret_cast<REBVAL *const *>(current);

        if (VAL_TYPE_RAW(cell) == REB_0) {

            // This is our "Alien" type that wants to get loaded (voids
            // cannot be legally loaded into blocks, by design).  Key
            // to his loading problem is that he wants to know whether
            // he is an explicit or implicit block type.  So that means
            // discerning between "foo bar" and "[foo bar]", which we
            // get through transcode which returns [foo bar] and
            // [[foo bar]] that discern the cases

            auto loadText = reinterpret_cast<REBYTE*>(
                cell->payload.handle.data.pointer // not really REB_HANDLE
            );

            // !!! Temporary: we can't let the GC see a REB_0 trash.
            // There will be a REB_LOADABLE and ET_LOADABLE type, so use
            // that when it arrives, but until then blank it.
            //
            Init_Unreadable_Blank(cell);

            // CAN raise errors and longjmp backwards on the C stack to
            // the `if (error)` case above!  These are the errors that
            // happen if the input is bad (unmatched parens, etc...)

            const char *rebol_hooks_utf8 = "rebol-hooks.cpp";
            REBSTR *rebol_hooks_filename = Intern_UTF8_Managed(
                cb_cast(rebol_hooks_utf8), strlen(rebol_hooks_utf8)
            );

            REBARR * transcoded = Scan_UTF8_Managed(
                rebol_hooks_filename, loadText, LEN_BYTES(loadText)
            );

            if (context) {
                // Binding Do_String did by default...except it only
                // worked with the user context.  Fell through to lib.

                REBCTX *c = VAL_CONTEXT(context->cell);
                REBCNT len = CTX_LEN(c);

                if (len > 0)
                    ASSERT_VALUE_MANAGED(ARR_HEAD(transcoded));

                Bind_Values_All_Deep(ARR_HEAD(transcoded), c);

                DECLARE_LOCAL (vali);
                Init_Integer(vali, len);

                Resolve_Context(
                    c,
                    Lib_Context,
                    vali,
                    FALSE, // !all
                    FALSE // !expand
                );
            }

            // Might think to use Append_Block here, but it's under
            // an #ifdef and apparently unused.  This is its definition.

            Insert_Series(
                SER(aggregate),
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

        current += sizeof(internal::Loadable);
    }

    if (constructOutTypeIn) {
        REBVAL *constructOutDatatypeIn = 
            constructOutTypeIn ? constructOutTypeIn->cell : nullptr;

        enum Reb_Kind resultType = VAL_TYPE(constructOutDatatypeIn);
        if (ANY_ARRAY(constructOutDatatypeIn)) {
            // They actually wanted a constructed value, and they wanted
            // effectively our aggregate...maybe with a different type.
            // Depending on how much was set in the "datatype in" we may
            // not have to rewrite the header bits (but Val_Inits do).

            Init_Any_Array(constructOutDatatypeIn, resultType, aggregate);

            // Val_Init makes aggregate a managed series, can't free it
            is_aggregate_managed = TRUE;
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
                ARR_HEAD(aggregate),
                nullptr // no parent
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
                /* Init_Error(
                    extraOut,
                    ::Error(RE_MISC);
                ); */
                panic (aggregate);

                extraOut.finishInit(engine);
                assert(hasType<Error>(extraOut));
                throw load_error {static_cast<Error>(extraOut)};
            }
            else if (resultType != VAL_TYPE(ARR_HEAD(aggregate))) {
                // Requested construct and value type was wrong
                Init_Error(
                    extraOut.cell,
                    ::Error(
                        RE_INVALID_ARG, // Make error code for this...
                        ARR_HEAD(aggregate)
                    )
                );
                extraOut.finishInit(engine);
                assert(hasType<Error>(extraOut));
                throw load_error {static_cast<Error>(extraOut)};
            }
            else {
                Move_Value(
                    constructOutDatatypeIn, KNOWN(ARR_HEAD(aggregate))
                );
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

        if (Generalized_Apply_Throws(
            applyOut->cell,
            applicand ? applicand->cell : nullptr,
            aggregate, // implicitly protected by the evaluator
            SPECIFIED // the aggregate is all REBVALs, fully specified
        )) {
            DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

            CATCH_THROWN(extraOut.cell, applyOut->cell);
            bool hasName = applyOut->tryFinishInit(engine);
            bool hasValue = extraOut->tryFinishInit(engine);
            throw evaluation_throw {
                hasValue ? optional<AnyValue>{extraOut} : nullopt,
                hasName ? optional<AnyValue>{*applyOut} : nullopt
            };
        }
    }

    DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

    assert(is_aggregate_managed == IS_ARRAY_MANAGED(aggregate));
    if (!is_aggregate_managed)
        Free_Array(aggregate);

    // It used to be required that we finalize the values before throwing
    // errors because (for instance) the tracking could be initialized.
    // That had to be changed because a Dont::Initialize was could construct
    // a type that could not survive an exception being thrown.  So we will
    // keep this finalization here just in case, because it should be safe now
    // to skip it in the case of an exception.

    if (constructOutTypeIn)
        constructOutTypeIn->finishInit(engine);

    if (applyOut) {
        //
        // `tryFinishInit()` will give back false if the cell was not a value
        // (e.g. an "unset") which cues a caller requesting a value that they
        // should make a `nullopt` for the `optional<AnyValue>` instead of
        // considering the bits "good".
        //
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
    RL_Move(cell, value.cell);
}

#if REN_CLASSLIB_QT == 1
internal::Loadable::Loadable (QString const & source) :
    Loadable (Dont::Initialize)
{
    String value {source};
    RL_Move(cell, value.cell);
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
    cell = reinterpret_cast<REBVAL*>(Alloc_Pairing(NULL));

    REBVAL *key = PAIRING_KEY(cell);
    Init_Blank(key);
    Init_Blank(cell);

    // Mark the created pairing so it will act as a "root".  The key and value
    // will be deep marked for GC.
    //
    SET_VAL_FLAG(key, NODE_FLAG_ROOT);
}


//
// DEBUGGING
//

#if !defined(NDEBUG)

void AnyValue::probe() const {
    PROBE(cell);
}

void AnyValue::validate() const {
    if (ANY_CONTEXT(cell))
        ASSERT_CONTEXT(VAL_CONTEXT(cell));
    else if (ANY_ARRAY(cell))
        ASSERT_ARRAY(VAL_ARRAY(cell));
    else if (ANY_SERIES(cell))
        ASSERT_SERIES(VAL_SERIES(cell));
}

#endif


//
// COMPARISON
//

bool AnyValue::isEqualTo(AnyValue const & other) const {
    // acts like REBNATIVE(equalq)

    DECLARE_LOCAL (cell_copy);
    Move_Value(cell_copy, cell);
    DECLARE_LOCAL (other_copy);
    Move_Value(other_copy, other.cell);

    // !!! Modifies arguments to coerce them for testing!
    return Compare_Modify_Values(cell_copy, other_copy, 0);
}

bool AnyValue::isSameAs(AnyValue const & other) const {
    // acts like REBNATIVE(sameq)

    DECLARE_LOCAL (cell_copy);
    Move_Value(cell_copy, cell);
    DECLARE_LOCAL (other_copy);
    Move_Value(other_copy, other.cell);

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
    assert(NOT_END(cell));

    // We no longer allow AnyValue to hold a void (unless specialization
    // using std::optional<AnyValue> represents unsets using that, which would
    // happen sometime later down the line when that optimization makes sense)
    // finishInit() is an inline wrapper that throws if this happens.
    //
    if (IS_VOID(cell))
        return false;

    return true;
}


void AnyValue::uninitialize() {

    Free_Pairing(cell);

    // drop refcount here

    origin = REN_ENGINE_HANDLE_INVALID;
}


void AnyValue::toCell_(
    REBVAL *out, optional<AnyValue> const & value
) noexcept {
    if (value == nullopt)
        Init_Void(out);
    else
        RL_Move(out, value->cell);
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
        userContext.cell,
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
    VAL_RESET_HEADER(cell, REB_0);
    cell->payload.handle.data.pointer = const_cast<char *>(sourceCstr);

    origin = REN_ENGINE_HANDLE_INVALID;
}


Loadable::Loadable (optional<AnyValue> const & value) :
    AnyValue (AnyValue::Dont::Initialize)
{
    if (value == nullopt)
        Init_Void(cell);
    else
        RL_Move(cell, value->cell);

    // We trust that the source value we copied from will stay alive and
    // prevent garbage collection... (review this idea)

    origin = REN_ENGINE_HANDLE_INVALID;
}


} // end namespace internal

} // end namespace ren
