#include "rencpp/values.hpp"
#include "rencpp/context.hpp"

#include "rencpp/red.hpp"


#define UNUSED(x) static_cast<void>(x)

namespace ren {


///
/// TYPE CHECKING AND INITIALIZATION
///
///

bool Value::isContext(RenCell *) const {
    throw std::runtime_error("context not implemented");
}



///
/// CONSTRUCTION
///

Context::Context (
    internal::Loadable const loadables[],
    size_t numLoadables,
    Context const * contextPtr,
    Engine * engine
) :
    Value (Dont::Initialize)
{
    throw std::runtime_error("Context::Context coming soon...");

    UNUSED(loadables);
    UNUSED(numLoadables);
    UNUSED(contextPtr);
    UNUSED(engine);
}


} // end namespace ren
