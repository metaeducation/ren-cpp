#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/blocks.hpp"
#include "rencpp/context.hpp"

#include "rencpp/rebol.hpp"

namespace ren {

///
/// TYPE DETECTION AND INITIALIZATION
///

bool Value::isBlock(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_BLOCK);
        return true;
    }
    return IS_BLOCK(&cell);
}

bool Value::isParen(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_PAREN);
        return true;
    }
    return IS_PAREN(&cell);
}

bool Value::isPath(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_PATH);
        return true;
    }
    return IS_PATH(&cell);
}

bool Value::isGetPath(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_GET_PATH);
        return true;
    }
    return IS_GET_PATH(&cell);
}

bool Value::isSetPath(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_SET_PATH);
        return true;
    }
    return IS_SET_PATH(&cell);
}

bool Value::isLitPath(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_LIT_PATH);
        return true;
    }
    return IS_LIT_PATH(&cell);
}

bool Value::isAnyBlock() const {
    return IS_BLOCK(&cell) or IS_PAREN(&cell) or IS_PATH(&cell)
        or IS_SET_PATH(&cell) or IS_GET_PATH(&cell) or IS_LIT_PATH(&cell);
}



///
/// BLOCK CONSTRUCTION
///


AnyBlock::AnyBlock (
    internal::Loadable const loadables[],
    size_t numLoadables,
    internal::CellFunction cellfun,
    Context const * contextPtr,
    Engine * engine
) :
    AnyBlock (Dont::Initialize)
{
    (this->*cellfun)(&this->cell);

    Context context = contextPtr ? *contextPtr : Context::runFinder(engine);

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
AnyBlock::AnyBlock (
    Value const values[],
    size_t numValues,
    internal::CellFunction cellfun,
    Context const * contextPtr,
    Engine * engine
) :
    AnyBlock (Dont::Initialize)
{
    (this->*cellfun)(&this->cell);


    Context context = contextPtr ? *contextPtr : Context::runFinder(engine);

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
