#include "rencpp/value.hpp"
#include "rencpp/atoms.hpp"
#include "rencpp/arrays.hpp"
#include "rencpp/function.hpp"

#include "rencpp/red.hpp"


namespace ren {


bool Value::isFunction() const {
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_FUNCTION;
}

void Function::finishInitSpecial(
    RenEngineHandle engine,
    Block const &, // spec
    RenShimPointer const & // shim
) {
    throw std::runtime_error("No way to make RedCell from C++ function yet.");

    Value::finishInit(engine);
}


} // end namespace ren
