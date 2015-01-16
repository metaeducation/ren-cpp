#include <stdexcept>
#include <vector>

#include "rencpp/values.hpp"
#include "rencpp/blocks.hpp"

#include "rencpp/rebol.hpp" // ren::internal::nodes


namespace ren {

///
/// COMPARISON
///

bool Value::isEqualTo(Value const & other) const {
    return Compare_Values(
        const_cast<REBVAL *>(&cell),
        const_cast<REBVAL *>(&other.cell),
        0 // REBNATIVE(equalq)
    );
}

bool Value::isSameAs(Value const & other) const {
    return Compare_Values(
        const_cast<REBVAL *>(&cell),
        const_cast<REBVAL *>(&other.cell),
        3 // REBNATIVE(sameq)
    );
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
        auto it = internal::nodes[engine.data].find(VAL_SERIES(&cell));
        if (it == internal::nodes[engine.data].end())
            internal::nodes[engine.data].emplace(VAL_SERIES(&cell), 1);
        else
            it->second++;
    #endif

    } else {
        refcountPtr = nullptr;
    }

    origin = engine;
}


///
/// BASIC STRING CONVERSIONS
///

#if REN_CLASSLIB_STD

std::string to_string(Value const & value) {
    const size_t defaultBufLen = 100;

    // Note .data() method is const on std::string.
    //    http://stackoverflow.com/questions/7518732/

    std::vector<char> buffer (defaultBufLen);

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

    auto result = std::string(buffer.data(), numBytes);
    return result;
}

#endif


#if REN_CLASSLIB_QT == 1

QString to_QString(Value const & value) {
    const size_t defaultBufLen = 100;

    QByteArray buffer (defaultBufLen, Qt::Uninitialized);

    size_t numBytes;

    // Note .data() method is const on std::string.
    //    http://stackoverflow.com/questions/7518732/

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

    buffer.truncate(numBytes);
    auto result = QString {buffer};
    return result;
}

#endif


///
/// "LOADABLE" VALUE(S) TYPE
///

namespace internal {

Loadable::Loadable () :
    Loadable (Block {})
{
}

Loadable::Loadable (char const * sourceCstr) :
    Value (Value::Dont::Initialize)
{
    // using REB_END as our "alien"
    VAL_SET(&cell, REB_END);
    VAL_HANDLE(&cell) =
        reinterpret_cast<ANYFUNC>(const_cast<char *>(sourceCstr));

    refcountPtr = nullptr;
    origin = REN_ENGINE_HANDLE_INVALID;
}

Loadable::Loadable (std::initializer_list<Loadable> loadables) :
    Value (Block(loadables))
{
}


} // end namespace internal

} // end namespace ren
