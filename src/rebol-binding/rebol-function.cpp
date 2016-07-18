#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/function.hpp"

#include "rebol-common.hpp"


namespace ren {

//
// Although the abstraction is that the RenShimPointer returns a RenResult,
// the native function pointers in Rebol do not do this.  It just happens
// that 0 maps to R_OUT, which is what we want.  The return conventions of
// Red are unlikely to match that...and maybe never give back an integer
// at all.  For the moment though we'll assume it does but the interpretation
// as some kind of error code for the hook seems more sensible.
//
static_assert(R_OUT == 0, "R_OUT must be 0 for RenShimPointer to work");


//
// TYPE DETECTION
//

//
// Really, from a user's point of view...shouldn't there only be ANY_FUNCTION?
// It's currently annoying if someone checks for taking a function and rejects
// closure.  The only user-facing issue would be whether you can get the
// body-of something or not, and that would seem to be addressable by something
// like NONE? BODY-OF :some-native ... also, source hiding may be a feature
// some users want of their own functions.
//

bool Function::isValid(RenCell const * cell) {
    return IS_FUNCTION(AS_C_REBVAL(cell));
}


#ifdef REN_RUNTIME


static REB_R Ren_Cpp_Dispatcher(struct Reb_Frame *f)
{
    REBARR *info = VAL_ARRAY(FUNC_BODY(f->func));

    RenEngineHandle engine;
    engine.data = cast(int, cast(REBUPT, VAL_HANDLE_DATA(ARR_AT(info, 0))));

    internal::RenShimPointer shim
        = cast(internal::RenShimPointer, VAL_HANDLE_CODE(ARR_AT(info, 1)));

    // !!! Note that this is a raw pointer to a C++ object.  Currently its
    // lifetime is until program end.  If these were to be GC'd, only the
    // shim would be able to do it...because only it would know how to cast
    // the std::function pointer to the right type for deletion.
    //
    void *cppfun = VAL_HANDLE_DATA(ARR_AT(info, 2));

    // f->arg has the 0-based arguments, f->out is the return
    //
    return (*shim)(
        AS_RENCELL(f->out),
        engine,
        cppfun,
        AS_RENCELL(f->args_head)
    );
}


//
// FUNCTION FINALIZER FOR EXTENSION
//

void Function::finishInitSpecial(
    RenEngineHandle engine,
    Block const & spec,
    internal::RenShimPointer shim,
    const void *cppfun // a std::function object, with varying type signatures
) {
    REBFUN *fun = Make_Function(
        Make_Paramlist_Managed_May_Fail(
            AS_C_REBVAL(spec.cell),
            MKF_KEYWORDS
        ),
        &Ren_Cpp_Dispatcher,
        NULL // no underlying function, this is fundamental
    );

    // The C++ function interface that is generated is typed specifically to
    // the parameters of the extension function.  This is what allows for
    // calling them in a way that looks like calling a C function ordinarily,
    // as well as getting type checking (int parameters for INTEGER!, strings
    // for STRING!, etc.)
    //
    // However, since all those functions have different type signatures, they
    // are different datatypes entirely.  They are funneled through a common
    // "unpacker" shim function, which is called by the dispatcher.

    REBARR *info = Make_Array(3);
    SET_HANDLE_DATA(
        Alloc_Tail_Array(info), cast(void*, cast(REBUPT, engine.data))
    );
    SET_HANDLE_CODE(Alloc_Tail_Array(info), cast(CFUNC*, shim));
    SET_HANDLE_DATA(Alloc_Tail_Array(info), cast(void*, cast(REBUPT, cppfun)));

    Val_Init_Block(FUNC_BODY(fun), info);

    *AS_REBVAL(cell) = *FUNC_VALUE(fun);

    AnyValue::finishInit(engine);
}

#endif

} // end namespace ren
