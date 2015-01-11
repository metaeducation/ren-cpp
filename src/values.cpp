//
// values.cpp
// This file is part of RenCpp
// Copyright (C) 2015 HostileFork.com
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

#include "rencpp/values.hpp"
#include "rencpp/exceptions.hpp"
#include "rencpp/runtime.hpp"
#include "rencpp/engine.hpp"
#include "rencpp/context.hpp"

namespace ren {


bool Value::needsRefcount() const {
    return Runtime::needsRefcount(cell);
}



// Even if asked not to initialize, we can't leave the type in a state where
// it cannot be safely freed.  A bad refcount pointer combined with bad data
// would be a problem.  Review this issue.

Value::Value (Dont const &) :
    refcountPtr {nullptr}
{
}



Value::operator bool() const {
    if (isUnset()) {
        // Is there a better way to throw the same "value is unset" error in a
        // way that matches what the runtime offers?
        (*this)();
        UNREACHABLE_CODE();
    }
    return not (isNone() or isFalse());
}




#if REN_CLASSLIB_STD
Value::operator std::string () const {
    const size_t defaultBufLen = 100;

    std::vector<char> buffer (defaultBufLen);

    size_t numBytes;

    // Note .data() method is const on std::string.
    //    http://stackoverflow.com/questions/7518732/

    switch (
        RenFormAsUtf8(
            origin, &cell, buffer.data(), defaultBufLen, &numBytes
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
                    origin,
                    &cell,
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


#if REN_CLASSLIB_QT
Value::operator QString () const {
    const size_t defaultBufLen = 100;

    QByteArray buffer (defaultBufLen, Qt::Uninitialized);

    size_t numBytes;

    // Note .data() method is const on std::string.
    //    http://stackoverflow.com/questions/7518732/

    switch (
        RenFormAsUtf8(
            origin, &cell, buffer.data(), defaultBufLen, &numBytes
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
                    origin,
                    &cell,
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
    auto result = QString(buffer);
    return result;
}

String::operator QString () const {
    // ridiculous way of doing it, but it's a start...and kind of impressive
    // that it works...
    QString result;
    for (ren::Character ch : *this)
        result += ch;
    return result;
}
#endif


std::ostream & operator<<(std::ostream & os, ren::Value const & value)
{
    return os << static_cast<std::string>(value);
}


AnyBlock::AnyBlock (
    internal::Loadable const loadables[],
    size_t numLoadables,
    internal::CellFunction cellfun,
    Context * context
) :
    AnyBlock (Dont::Initialize)
{
    (this->*cellfun)(&this->cell);

    if (not context)
        context = &Context::runFinder(nullptr);

    constructOrApplyInitialize(
        context->getEngine().getHandle(),
        context->getHandle(),
        nullptr,
        loadables,
        numLoadables,
        this, // Do construct
        nullptr // Don't apply
    );
}


AnyWord::AnyWord (
    char const * cstr,
    internal::CellFunction cellfun,
    Context * context
) :
    Value (Dont::Initialize)
{
    (this->*cellfun)(&this->cell);

    if (not context)
        context = &Context::runFinder(nullptr);

    internal::Loadable loadable {cstr};

    constructOrApplyInitialize(
        context->getEngine().getHandle(),
        context->getHandle(),
        nullptr,
        &loadable,
        1,
        // Do construct
        this,
        // Don't apply
        nullptr
    );
}



#if REN_CLASSLIB_QT
AnyWord::AnyWord (
    QString const & str,
    internal::CellFunction cellfun,
    Context * context
) :
    Value (Dont::Initialize)
{
    (this->*cellfun)(&this->cell);

    // can't return char * without intermediate
    // http://stackoverflow.com/questions/17936160/
    QByteArray array = str.toLocal8Bit();
    internal::Loadable loadable {array.data()};

    if (not context)
        context = &Context::runFinder(nullptr);

    constructOrApplyInitialize(
        context->getEngine().getHandle(),
        context->getHandle(),
        nullptr,
        &loadable,
        1,
        // Do construct
        this,
        // Don't apply
        nullptr
    );
}
#endif


AnyString::AnyString (
    char const * cstr,
    internal::CellFunction cellfun,
    Engine * engine
) :
    Series (Dont::Initialize)
{
    (this->*cellfun)(&this->cell);

    if (not engine)
        engine = &Engine::runFinder();

    internal::Loadable loadable {cstr};

    constructOrApplyInitialize(
        engine->getHandle(),
        REN_CONTEXT_HANDLE_INVALID,
        nullptr,
        &loadable,
        1,
        this, // Do construct
        nullptr // Don't apply
    );
}


///
/// GENERALIZED APPLY
///

Value Value::apply_(
    internal::Loadable const loadables[],
    size_t numLoadables,
    Context * context
) const {
    Value result {Dont::Initialize};

    if (context == nullptr)
        context = &Context::runFinder(nullptr);

    constructOrApplyInitialize(
        context->getEngine().getHandle(),
        context->getHandle(),
        this,
        loadables,
        numLoadables,
        nullptr, // don't construct
        &result // do apply
    );

    return result;
}


Value Value::apply(
    std::initializer_list<internal::Loadable> loadables,
    Context * context
) const {
    // This one has to be in the implementation file because it appears in
    // Value, using Loadable, which is derived from Value...
    return apply_(loadables.begin(), loadables.size(), context);
}


void Value::constructOrApplyInitialize(
    RenEngineHandle engine,
    RenContextHandle context,
    Value const * applicand,
    internal::Loadable const loadables[],
    size_t numLoadables,
    Value * constructOutTypeIn,
    Value * applyOut
) {
    Value errorOut {Value::Dont::Initialize};

    auto result = ::RenConstructOrApply(
        engine,
        context,
        &applicand->cell,
        numLoadables != 0 ? &loadables[0].cell : nullptr,
        numLoadables,
        sizeof(internal::Loadable),
        constructOutTypeIn ? &constructOutTypeIn->cell : nullptr,
        applyOut ? &applyOut->cell : nullptr,
        &errorOut.cell
    );

    switch (result) {
        case REN_SUCCESS:
            break;

        case REN_CONSTRUCT_ERROR:
        case REN_APPLY_ERROR:
            errorOut.finishInit(engine);
            throw evaluation_error(errorOut);
            break;

        case REN_EVALUATION_CANCELLED:
            throw evaluation_cancelled();

        case REN_EVALUATION_EXITED:
            throw exit_command(VAL_INT32(&errorOut.cell));

        default:
            throw std::runtime_error("Unknown error in RenConstructOrApply");
    }

    // It used to be required that we finalize the values before throwing
    // errors because (for instance) the refcount could be initialized.
    // That had to be changed because a Dont::Initialize was could construct
    // a type that could not survive an exception being thrown.  So we will
    // keep this finalization here just in case, because it should be safe now
    // to skip it in the case of an exception.

    if (constructOutTypeIn)
        constructOutTypeIn->finishInit(engine);

    if (applyOut)
        applyOut->finishInit(engine);
}


} // end namespace ren
