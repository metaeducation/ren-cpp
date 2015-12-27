#include "rencpp/value.hpp"
#include "rencpp/context.hpp"

#include "rencpp/red.hpp"


#define UNUSED(x) static_cast<void>(x)

namespace ren {


///
/// TYPE CHECKING AND INITIALIZATION
///
///

bool AnyContext::isValid(RenCell const &) {
    throw std::runtime_error("context not implemented");
}



///
/// CONSTRUCTION
///

AnyContext::AnyContext (
    internal::Loadable const loadables[],
    size_t numLoadables,
    AnyContext const * contextPtr,
    Engine * engine
) :
    AnyValue (Dont::Initialize)
{
    throw std::runtime_error("AnyContext::AnyContext coming soon...");

    UNUSED(loadables);
    UNUSED(numLoadables);
    UNUSED(contextPtr);
    UNUSED(engine);
}


} // end namespace ren
