#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/context.hpp"
#include "rencpp/engine.hpp"

namespace ren {

//
// TYPE DETECTION AND INITIALIZATION
//

bool Value::isContext(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_OBJECT);
        return true;
    }
    return IS_OBJECT(&cell);
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
    Value (Dont::Initialize)
{
    isContext(&cell); // CellFunction; writes type signature into cell

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
    Value const values[],
    size_t numValues,
    Context const * contextPtr,
    Engine * engine
) :
    Value (Dont::Initialize)
{
    VAL_SET(&cell, REB_OBJECT);

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
