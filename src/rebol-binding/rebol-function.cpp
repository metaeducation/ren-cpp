#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/function.hpp"


namespace ren {

///
/// TYPE DETECTION
///

//
// Really, from a user's point of view...shouldn't there only be ANY_FUNCTION?
// It's currently annoying if someone checks for taking a function and rejects
// closure.  The only user-facing issue would be whether you can get the
// body-of something or not, and that would seem to be addressable by something
// like NONE? BODY-OF :some-native ... also, source hiding may be a feature
// some users want of their own functions.
//

bool Value::isFunction() const {
    return IS_FUNCTION(&cell)
        or IS_NATIVE(&cell)
        or IS_CLOSURE(&cell)
        or IS_ACTION(&cell);
}



///
/// FUNCTION FINALIZER FOR EXTENSION
///

void Function::finishInitSpecial(
    RenEngineHandle engine,
    Block const & spec,
    RenShimPointer const & shim
) {
    Make_Native(&cell, VAL_SERIES(&spec.cell), shim, REB_NATIVE);

    Value::finishInit(engine);
}

} // end namespace ren
