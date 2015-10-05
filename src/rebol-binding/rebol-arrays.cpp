#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/arrays.hpp"
#include "rencpp/context.hpp"

#include "rencpp/rebol.hpp"

namespace ren {

//
// TYPE DETECTION AND INITIALIZATION
//

bool AnyValue::isBlock(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_BLOCK);
        return true;
    }
    return IS_BLOCK(&cell);
}

bool AnyValue::isGroup(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_PAREN);
        return true;
    }
    return IS_PAREN(&cell);
}

bool AnyValue::isPath(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_PATH);
        return true;
    }
    return IS_PATH(&cell);
}

bool AnyValue::isGetPath(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_GET_PATH);
        return true;
    }
    return IS_GET_PATH(&cell);
}

bool AnyValue::isSetPath(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_SET_PATH);
        return true;
    }
    return IS_SET_PATH(&cell);
}

bool AnyValue::isLitPath(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_LIT_PATH);
        return true;
    }
    return IS_LIT_PATH(&cell);
}

bool AnyValue::isAnyArray() const {
    return IS_BLOCK(&cell) or IS_PAREN(&cell) or IS_PATH(&cell)
        or IS_SET_PATH(&cell) or IS_GET_PATH(&cell) or IS_LIT_PATH(&cell);
}



//
// BLOCK CONSTRUCTION
//


AnyArray::AnyArray (
    internal::Loadable const loadables[],
    size_t numLoadables,
    internal::CellFunction cellfun,
    Context const * contextPtr,
    Engine * engine
) :
    AnyArray (Dont::Initialize)
{
    (this->*cellfun)(&this->cell);

    Context context = contextPtr ? *contextPtr : Context::current(engine);

    constructOrApplyInitialize(
        context.getEngine(),
        &context,
        nullptr, // no applicand
        loadables,
        numLoadables,
        this, // Do construct
        nullptr // Don't apply
    );
}


// TBD: Finish version where you can use values directly as an array
/*
AnyArray::AnyArray (
    AnyValue const values[],
    size_t numValues,
    internal::CellFunction cellfun,
    Context const * contextPtr,
    Engine * engine
) :
    AnyArray (Dont::Initialize)
{
    (this->*cellfun)(&this->cell);


    Context context = contextPtr ? *contextPtr : Context::current(engine);

    constructOrApplyInitialize(
        context.getEngine(),
        &context,
        nullptr, // no applicand
        loadables,
        numLoadables,
        this, // Do construct
        nullptr // Don't apply
    );
}
*/



} // end namespace ren
