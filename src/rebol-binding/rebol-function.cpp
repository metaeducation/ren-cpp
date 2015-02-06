#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/function.hpp"


namespace ren {

//
// Although the abstraction is that the RenShimPointer returns a RenResult,
// the native function pointers in Rebol do not do this.  It just happens
// that 0 maps to R_RET, which is what we want.  The return conventions of
// Red are unlikely to match that...and maybe never give back an integer
// at all.  For the moment though we'll assume it does but the interpretation
// as some kind of error code for the hook seems more sensible.
//
static_assert(R_RET == 0, "R_RET must be 0 for RenShimPointer to work");


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
    return IS_ANY_FUNCTION(&cell);
}


#ifdef REN_RUNTIME

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

#endif

} // end namespace ren
