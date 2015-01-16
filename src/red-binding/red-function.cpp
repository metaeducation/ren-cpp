#include "rencpp/value.hpp"
#include "rencpp/indivisibles.hpp"
#include "rencpp/blocks.hpp"
#include "rencpp/function.hpp"

#include "rencpp/red.hpp"


namespace ren {


void Function::finishInitSpecial(
    RenEngineHandle engine,
    Block const &, // spec
    RenShimPointer const & // shim
) {
    throw std::runtime_error("No way to make RedCell from C++ function yet.");

    Value::finishInit(engine);
}


} // end namespace ren
