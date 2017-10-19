#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/error.hpp"
#include "rencpp/engine.hpp"

#include "common.hpp"


namespace ren {

//
// TYPE DETECTION AND INITIALIZATION
//

bool Error::isValid(REBVAL const * cell) {
    return IS_ERROR(cell);
}



//
// CONSTRUCTION
//

Error::Error (const char * msg, Engine * engine) :
    AnyContext_ (Dont::Initialize)
{
    VAL_RESET_HEADER(cell, REB_ERROR);

    if (engine == nullptr)
        engine = &Engine::runFinder();

    std::string array (
        "#[error! [code: _ type: 'User id: 'message message: "
    );

    array += '{';
    array += msg;
    array += '}';

    // the shim could adjust the where and say what function threw it?
    // file/line number optional?

    array += "]]";

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

} // end namespace ren
