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


ren::Value ren::Value::apply(
    Context * context,
    internal::Loadable * loadablesPtr,
    size_t numLoadables
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


AnyBlock::AnyBlock (
    Context & context,
    internal::Loadable * loadablesPtr,
    size_t numLoadables,
    bool (Value::*validMemFn)(RenCell *) const
) :
    AnyBlock (Dont::Initialize)
{
    (this->*validMemFn)(&this->cell);

    context.constructOrApplyInitialize(
        nullptr,
        loadablesPtr,
        numLoadables,
        this, // Do construct
        nullptr // Don't apply
    );
}


AnyBlock::AnyBlock (
    internal::Loadable * loadablesPtr,
    size_t numLoadables,
    bool (Value::*validMemFn)(RenCell *) const
) :
    AnyBlock (
        Context::runFinder(nullptr),
        loadablesPtr,
        numLoadables,
        validMemFn
    )
{
}


AnyWord::AnyWord (
    Context & context,
    char const * cstr,
    bool (Value::*validMemFn)(RenCell *) const
) :
    Value (Dont::Initialize)
{
    internal::Loadable loadable (cstr);
    (this->*validMemFn)(&this->cell);

    context.constructOrApplyInitialize(
        nullptr,
        &loadable,
        1,
        // Do construct
        this,
        // Don't apply
        nullptr
    );
}


AnyWord::AnyWord (
    char const * cstr,
    bool (Value::*validMemFn)(RenCell *) const
) :
    AnyWord (Context::runFinder(nullptr), cstr, validMemFn)
{
}


#if REN_CLASSLIB_QT
AnyWord::AnyWord (
    Context & context,
    QString const & str,
    bool (Value::*validMemFn)(RenCell *) const
) :
    Value (Dont::Initialize)
{
    // can't return char * without intermediate
    // http://stackoverflow.com/questions/17936160/
    QByteArray array = str.toLocal8Bit();
    char const * buffer = array.data();

    internal::Loadable loadable (buffer);
    (this->*validMemFn)(&this->cell);

    context.constructOrApplyInitialize(
        nullptr,
        &loadable,
        1,
        // Do construct
        this,
        // Don't apply
        nullptr
    );
}


AnyWord::AnyWord (
    QString const & str,
    bool (Value::*validMemFn)(RenCell *) const
) :
    AnyWord (Context::runFinder(nullptr), str, validMemFn)
{
}
#endif


AnyString::AnyString (
    Engine & engine,
    char const * cstr,
    bool (Value::*validMemFn)(RenCell *) const
) :
    Series (Dont::Initialize)
{
    (this->*validMemFn)(&this->cell);

    internal::Loadable loadable (cstr);

    Context::runFinder(&engine).constructOrApplyInitialize(
        nullptr,
        &loadable,
        1,
        this, // Do construct
        nullptr // Don't apply
    );
}


AnyString::AnyString (
    char const * cstr,
    bool (Value::*validMemFn)(RenCell *) const
) :
    AnyString (Engine::runFinder(), cstr, validMemFn)
{
}

} // end namespace ren
