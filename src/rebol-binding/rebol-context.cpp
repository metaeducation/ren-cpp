#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/context.hpp"
#include "rencpp/engine.hpp"

#include "rebol-common.hpp"


namespace ren {

//
// TYPE DETECTION
//

bool AnyContext::isValid(RenCell const * cell) {
    return IS_OBJECT(AS_C_REBVAL(cell));
}



//
// CONSTRUCTION
//

AnyContext::AnyContext (
    internal::Loadable const loadables[],
    size_t numLoadables,
    internal::CellFunction cellfun,
    AnyContext const * contextPtr,
    Engine * engine
) :
    AnyValue (Dont::Initialize)
{
    (*cellfun)(this->cell);

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

AnyContext::AnyContext (
    AnyValue const values[],
    size_t numValues,
    internal::CellFunction cellfun,
    AnyContext const * contextPtr,
    Engine * engine
) :
    AnyValue (Dont::Initialize)
{
    (*cellfun)(this->cell);

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

//
// TYPE HEADER INITIALIZATION
//

void AnyContext::initObject(RenCell * cell) {
    VAL_RESET_HEADER(AS_REBVAL(cell), REB_OBJECT);
}

void AnyContext::initError(RenCell * cell) {
    VAL_RESET_HEADER(AS_REBVAL(cell), REB_ERROR);
}

} // end namespace ren
