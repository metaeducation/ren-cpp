#include <array>
#include <stdexcept>
#include <vector>

#include "rencpp/value.hpp"
#include "rencpp/blocks.hpp"
#include "rencpp/context.hpp"

#include "rencpp/rebol.hpp" // ren::internal::nodes


namespace ren {

//
// COMPARISON
//

bool Value::isEqualTo(Value const & other) const {
    // acts like REBNATIVE(equalq)

    REBVAL cell_copy;
    cell_copy = cell;
    REBVAL other_copy;
    other_copy = other.cell;

    // !!! Modifies arguments to coerce them for testing!
    return Compare_Modify_Values(&cell_copy, &other_copy, 0);
}

bool Value::isSameAs(Value const & other) const {
    // acts like REBNATIVE(sameq)

    REBVAL cell_copy;
    cell_copy = cell;
    REBVAL other_copy;
    other_copy = other.cell;

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

void Value::finishInit(RenEngineHandle engine) {
    if (needsRefcount()) {
        refcountPtr = new RefcountType (1);

    #ifndef NDEBUG
        REBSER * series = VAL_SERIES(&cell);
        auto it = internal::nodes[engine.data].find(series);
        if (it == internal::nodes[engine.data].end())
            internal::nodes[engine.data].emplace(series, 1);
        else
            it->second++;
    #endif
    }
    else
        refcountPtr = nullptr;

    origin = engine;
}


Value Value::copy(bool deep) const {

    // It seems the only way to call an action is to put the arguments it
    // expects onto the stack :-/  For instance in the dispatch of A_COPY
    // we see it uses D_REF(ARG_COPY_DEEP) in the block handler to determine
    // whether to copy deeply.  So there is no deep flag to Copy_Value.  :-/
    // Exactly what the incantation would be can be figured out another
    // day but it would look something(?) like this commented out code...

  /*
    auto saved_DS_TOP = DS_TOP;

    Value result (Dont::Initialize);

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
        &userContext.cell,
        VAL_OBJ_FRAME(Get_System(SYS_CONTEXTS, CTX_USER))
    );
    userContext.finishInit(origin);

    std::array<internal::Loadable, 2> loadables = {{
        deep ? "copy/deep" : "copy",
        *this
    }};

    Value result (Dont::Initialize);

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

std::string to_string(Value const & value) {
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

QString to_QString(Value const & value) {
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
            buffer.resize(numBytes);

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

    buffer.truncate(numBytes);
    auto result = QString {buffer};
    return result;
}

#endif


//
// "LOADABLE" VALUE(S) TYPE
//

namespace internal {

Loadable::Loadable (char const * sourceCstr) :
    Value (Value::Dont::Initialize)
{
    // using REB_END as our "alien"
    VAL_SET(&cell, REB_END);
    VAL_HANDLE_DATA(&cell) = const_cast<char *>(sourceCstr);

    refcountPtr = nullptr;
    origin = REN_ENGINE_HANDLE_INVALID;
}


} // end namespace internal

} // end namespace ren
