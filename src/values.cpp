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
#include "rencpp/blocks.hpp"
#include "rencpp/strings.hpp"
#include "rencpp/words.hpp"
#include "rencpp/error.hpp"
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

Value::Value (Dont) :
    refcountPtr {nullptr}
{
}



Value::operator bool() const {
    if (isUnset()) {
        // Is there a better way to throw the same "value is unset" error in a
        // way that matches what the runtime offers?
        this->apply();
        UNREACHABLE_CODE();
    }
    return not (isNone() or isFalse());
}




#if REN_CLASSLIB_STD
std::string to_string(Value const & value) {
    const size_t defaultBufLen = 100;

    std::vector<char> buffer (defaultBufLen);

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

    auto result = std::string(buffer.data(), numBytes);
    return result;
}
#endif


#if REN_CLASSLIB_QT
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

AnyString::operator QString () const {
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
    return os << to_string(value);
}


AnyBlock::AnyBlock (
    internal::Loadable const loadables[],
    size_t numLoadables,
    internal::CellFunction cellfun,
    Context const * contextPtr,
    Engine * engine
) :
    AnyBlock (Dont::Initialize)
{
    (this->*cellfun)(&this->cell);

    Context context = contextPtr ? *contextPtr : Context::runFinder(engine);

    constructOrApplyInitialize(
        context.getEngine(),
        &context,
        nullptr, // no applicand
        loadables,
        numLoadables,
        this, // Do construct
        nullptr // Don't apply
    );
}


// TBD: Finish version where you can use values directly as an array
/*
AnyBlock::AnyBlock (
    Value const values[],
    size_t numValues,
    internal::CellFunction cellfun,
    Context const * contextPtr,
    Engine * engine
) :
    AnyBlock (Dont::Initialize)
{
    (this->*cellfun)(&this->cell);


    Context context = contextPtr ? *contextPtr : Context::runFinder(engine);

    constructOrApplyInitialize(
        context.getEngine(),
        &context,
        nullptr, // no applicand
        loadables,
        numLoadables,
        this, // Do construct
        nullptr // Don't apply
    );
}
*/


AnyWord::AnyWord (
    char const * spelling,
    internal::CellFunction cellfun,
    Context const * contextPtr,
    Engine * engine
) :
    Value (Dont::Initialize)
{
    (this->*cellfun)(&this->cell);

    std::string array;

    // Note: wouldn't be able to return char * without intermediate
    // http://stackoverflow.com/questions/17936160/

    if (isWord()) {
        array += spelling;
    }
    else if (isSetWord()) {
        array += spelling;
        array += ':';
    }
    else if (isGetWord()) {
        array += ':';
        array += spelling;
    }
    else if (isLitWord()) {
        array += '\'';
        array += spelling;
    }
    else if (isRefinement()) {
        array += '/';
        array += spelling;
    }
    else
        UNREACHABLE_CODE();

    internal::Loadable loadable = array.data();

    Context context = contextPtr ? *contextPtr : Context::runFinder(engine);

    constructOrApplyInitialize(
        context.getEngine(),
        &context,
        nullptr, // no applicand
        &loadable,
        1,
        this, // do construct
        nullptr // don't apply
    );
}



#if REN_CLASSLIB_QT
AnyWord::AnyWord (
    QString const & spelling,
    internal::CellFunction cellfun,
    Context const * contextPtr,
    Engine * engine
) :
    Value (Dont::Initialize)
{
    (this->*cellfun)(&this->cell);

    // can't return char * without intermediate
    // http://stackoverflow.com/questions/17936160/
    QByteArray array;

    if (isWord()) {
        array += spelling.toLocal8Bit();
    }
    else if (isSetWord()) {
        array += spelling.toLocal8Bit();
        array += ':';
    }
    else if (isGetWord()) {
        array += ':';
        array += spelling.toLocal8Bit();
    }
    else if (isLitWord()) {
        array += '\'';
        array += spelling.toLocal8Bit();
    }
    else if (isRefinement()) {
        array += '/';
        array += spelling.toLocal8Bit();
    }
    else
        UNREACHABLE_CODE();

    internal::Loadable loadable = array.data();

    Context context = contextPtr ? *contextPtr : Context::runFinder(engine);

    constructOrApplyInitialize(
        context.getEngine(),
        &context,
        nullptr, // no applicand
        &loadable,
        1,
        this, // do construct
        nullptr // don't apply
    );
}
#endif


AnyString::AnyString (
    char const * spelling,
    internal::CellFunction cellfun,
    Engine * engine
) :
    Series (Dont::Initialize)
{
    (this->*cellfun)(&this->cell);

    if (not engine)
        engine = &Engine::runFinder();

    std::string array;

    if (isString()) {
        array += '{';
        array += spelling;
        array += '}';
    }
    else if (isTag()) {
        array += '<';
        array += spelling;
        array += '>';
    }
    else
        UNREACHABLE_CODE();

    internal::Loadable loadable = array.data();

    constructOrApplyInitialize(
        engine->getHandle(),
        nullptr, // no context
        nullptr, // no applicand
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
    Context const * contextPtr,
    Engine * engine
) const {
    Value result {Dont::Initialize};

    Context context = contextPtr ? *contextPtr : Context::runFinder(engine);

    constructOrApplyInitialize(
        context.getEngine(),
        &context,
        this, // no applicand
        loadables,
        numLoadables,
        nullptr, // don't construct
        &result // do apply
    );

    return result;
}


Value Value::apply(
    std::initializer_list<internal::Loadable> loadables,
    Context const & context
) const {
    // This one has to be in the implementation file because it appears in
    // Value, using Loadable, which is derived from Value...
    return apply_(loadables.begin(), loadables.size(), &context, nullptr);
}

Value Value::apply(
    std::initializer_list<internal::Loadable> loadables,
    Engine * engine
) const {
    // This one has to be in the implementation file because it appears in
    // Value, using Loadable, which is derived from Value...
    return apply_(loadables.begin(), loadables.size(), nullptr, engine);
}


void Value::constructOrApplyInitialize(
    RenEngineHandle engine,
    Context const * context,
    Value const * applicand,
    internal::Loadable const loadables[],
    size_t numLoadables,
    Value * constructOutTypeIn,
    Value * applyOut
) {
    Error errorOut {Value::Dont::Initialize};

    auto result = ::RenConstructOrApply(
        engine,
        &context->cell,
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


Value Series::operator[](size_t index) const {
    // Terrible placeholder implementation (helps with testing in any case)

    if (index > length())
        return ren::none;

    size_t count = index;
    auto it = begin();
    while (count > 1) {
        count--;
        it++;
    }

    return *it;
}


} // end namespace ren
