#include "rencpp/values.hpp"
#include "rencpp/runtime.hpp"
#include "rencpp/context.hpp"


namespace ren {


bool Value::needsRefcount() const {
    return Runtime::needsRefcount(cell);
}


Value::Value (Engine & engine, RenCell const & cell) :
    cell (cell)
{
    finishInit(engine.handle);
}


void Value::finishInit(RenEngineHandle engineHandle) {
    if (needsRefcount()) {
        refcountPtr = new RefcountType (1);
    } else {
        refcountPtr = nullptr;
    }
    origin = engineHandle;
}



Value::Value () :
    Value (Engine::runFinder())
{
}


Value::Value (Dont const &)
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
) {
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


None::None () :
    None (Engine::runFinder())
{
}

None none {};


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
