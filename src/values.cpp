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

#include "rencpp/values.hpp"
#include "rencpp/runtime.hpp"
#include "rencpp/context.hpp"


namespace ren {


bool Value::needsRefcount() const {
    return Runtime::needsRefcount(cell);
}


Value::Value () :
    Value (Engine::runFinder())
{
}


// Even if asked not to initialize, we can't leave the type in a state where
// it cannot be safely freed.  A bad refcount pointer combined with bad data
// would be a problem.  Review this issue.

Value::Value (Dont const &) :
    refcountPtr {nullptr}
{
}


Value::Value (none_t const &) :
    Value (Engine::runFinder(), none)
{
}


Value::Value (bool const & someBool) :
    Value (Engine::runFinder(), someBool)
{
}


Value::operator bool() const {
    if (isUnset()) {
        // Is there a better way to throw the "value is unset" error in a
        // way that matches what the runtime offers?
        (*this)();
    }
    return (not isNone()) and (not isFalse());
}


Value::Value (int const & someInt) :
    Value (Engine::runFinder(), someInt)
{
}


Value::Value (double const & someDouble) :
    Value (Engine::runFinder(), someDouble)
{
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
#endif


std::ostream & operator<<(std::ostream & os, ren::Value const & value)
{
    return os << static_cast<std::string>(value);
}


Value Value::apply(
    RenCell * loadablesPtr,
    size_t numLoadables,
    Context * context
) const {
    Value result {Dont::Initialize};
    if (context == nullptr)
        context = &Context::runFinder(nullptr);
    context->constructOrApplyInitialize(
        this,
        loadablesPtr,
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
    RenCell * load = const_cast<RenCell *>(&loadables.begin()->cell);
    return apply(load, loadables.size(), context);
}

AnyBlock::AnyBlock (
    RenCell * loadablesPtr,
    size_t numLoadables,
    internal::CellFunction cellfun,
    Context * context
) :
    AnyBlock (Dont::Initialize)
{
    (this->*cellfun)(&this->cell);

    Context & actualContext = context ? *context : Context::runFinder(nullptr);
    actualContext.constructOrApplyInitialize(
        nullptr,
        loadablesPtr,
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

    internal::Loadable loadable (cstr);

    Context & actualContext = context ? *context : Context::runFinder(nullptr);
    actualContext.constructOrApplyInitialize(
        nullptr,
        &loadable.cell,
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
    internal::Loadable loadable (array.data());

    Context & actualContext = context ? *context : Context::runFinder(nullptr);
    actualContext.constructOrApplyInitialize(
        nullptr,
        &loadable.cell,
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

    internal::Loadable loadable (cstr);

    Context::runFinder(engine).constructOrApplyInitialize(
        nullptr,
        &loadable.cell,
        1,
        this, // Do construct
        nullptr // Don't apply
    );
}

} // end namespace ren
