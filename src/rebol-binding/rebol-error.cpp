#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/error.hpp"
#include "rencpp/engine.hpp"


namespace ren {

///
/// TYPE DETECTION AND INITIALIZATION
///

bool Value::isError() const {
    return IS_ERROR(&cell);
}



///
/// CONSTRUCTION
///

Error::Error (const char * msg, Engine * engine) :
    Value (Dont::Initialize)
{
    VAL_SET(&this->cell, REB_ERROR);

    if (not engine)
        engine = &Engine::runFinder();

    std::string array {"#[error! [code: 800 type: 'User id: 'message arg1: "};

    array += '"';
    array += msg;
    array += '"';

    // the shim could adjust the where and say what function threw it?
    // file/line number optional?

    array += " arg2: none arg3: none near: {ren::Error} where: none]]";

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
