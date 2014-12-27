#include "rencpp/values.hpp"
#include "rencpp/runtime.hpp"
#include "rencpp/context.hpp"


namespace ren {


bool Value::needsRefcount() const {
    return Runtime::needsRefcount(cell);
}


Value::Value (RenEngineHandle engine, RenCell const & cell) :
    cell (cell)
{
    finishInit(engine);
}


Value::Value (Engine & engine, RenCell const & cell) :
    cell (cell)
{
    finishInit(engine.handle);
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


Value::Value (int const & someInt) :
    Value (Engine::runFinder(), someInt)
{
}


Value::Value (double const & someDouble) :
    Value (Engine::runFinder(), someDouble)
{
}


Value::operator std::string () const {
    return Runtime::form(*this);
}


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


AnyString::AnyString (
    Context & context,
    char const * cstr,
    bool (Value::*validMemFn)(RenCell *) const
) :
    Series (Dont::Initialize)
{
    (this->*validMemFn)(&this->cell);

    internal::Loadable loadable (cstr);

    context.constructOrApplyInitialize(
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
    AnyString (Context::runFinder(nullptr), cstr, validMemFn)
{
}

} // end namespace ren
