#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/context.hpp"
#include "rencpp/engine.hpp"

#include "rebol-common.hpp"


namespace ren {

//
// TYPE DETECTION
//

bool Context::isValid(RenCell const & cell) {
    return IS_OBJECT(AS_C_REBVAL(&cell));
}



//
// CONSTRUCTION
//

Context::Context (
    internal::Loadable const loadables[],
    size_t numLoadables,
    Context const * contextPtr,
    Engine * engine
) :
    AnyValue (Dont::Initialize)
{
    VAL_RESET_HEADER(AS_REBVAL(&cell), REB_OBJECT);

    // Here, a null context pointer means null.  No finder is invoked.

    RenEngineHandle realEngine = contextPtr ? contextPtr->getEngine() :
        (engine ? engine->getHandle() : Engine::runFinder().getHandle());

    constructOrApplyInitialize(
        realEngine,
        contextPtr,
        nullptr, // no applicand
        loadables,
        numLoadables,
        this, // Do construct
        nullptr // Don't apply
    );
}


// TBD: Finish version where you can use values directly as an array
/*

Context::Context (
    AnyValue const values[],
    size_t numValues,
    Context const * contextPtr,
    Engine * engine
) :
    AnyValue (Dont::Initialize)
{
    VAL_RESET_HEADER(&cell, REB_OBJECT);

    // Here, a null context pointer means null.  No finder is invoked.

    RenEngineHandle realEngine = contextPtr ? contextPtr->getEngine() :
        (engine ? engine->getHandle() : Engine::runFinder().getHandle());

    constructOrApplyInitialize(
        realEngine,
        nullptr, // no applicand
        loadables,
        numLoadables,
        this, // Do construct
        nullptr // Don't apply
    );
}

*/


} // end namespace ren
