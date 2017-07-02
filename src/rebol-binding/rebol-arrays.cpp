#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/arrays.hpp"
#include "rencpp/context.hpp"

#include "rencpp/rebol.hpp"

#include "rebol-common.hpp"


namespace ren {

//
// TYPE DETECTION
//

bool Block::isValid(RenCell const * cell) {
    return IS_BLOCK(AS_C_REBVAL(cell));
}

bool Group::isValid(RenCell const * cell) {
    return IS_GROUP(AS_C_REBVAL(cell));
}

bool Path::isValid(RenCell const * cell) {
    return IS_PATH(AS_C_REBVAL(cell));
}

bool GetPath::isValid(RenCell const * cell) {
    return IS_GET_PATH(AS_C_REBVAL(cell));
}

bool SetPath::isValid(RenCell const * cell) {
    return IS_SET_PATH(AS_C_REBVAL(cell));
}

bool LitPath::isValid(RenCell const * cell) {
    return IS_LIT_PATH(AS_C_REBVAL(cell));
}

bool AnyArray::isValid(RenCell const * cell) {
    return IS_BLOCK(AS_C_REBVAL(cell))
        || IS_GROUP(AS_C_REBVAL(cell))
        || IS_PATH(AS_C_REBVAL(cell))
        || IS_SET_PATH(AS_C_REBVAL(cell))
        || IS_GET_PATH(AS_C_REBVAL(cell))
        || IS_LIT_PATH(AS_C_REBVAL(cell));
}


//
// TYPE HEADER INITIALIZATION
//

void AnyArray::initBlock(RenCell * cell) {
    VAL_RESET_HEADER(AS_REBVAL(cell), REB_BLOCK);
}

void AnyArray::initGroup(RenCell * cell) {
    VAL_RESET_HEADER(AS_REBVAL(cell), REB_GROUP);
}

void AnyArray::initPath(RenCell * cell) {
    VAL_RESET_HEADER(AS_REBVAL(cell), REB_PATH);
}

void AnyArray::initGetPath(RenCell * cell) {
    VAL_RESET_HEADER(AS_REBVAL(cell), REB_GET_PATH);
}

void AnyArray::initSetPath(RenCell * cell) {
    VAL_RESET_HEADER(AS_REBVAL(cell), REB_SET_PATH);
}

void AnyArray::initLitPath(RenCell * cell) {
    VAL_RESET_HEADER(AS_REBVAL(cell), REB_LIT_PATH);
}


//
// BLOCK CONSTRUCTION
//


AnyArray::AnyArray (
    internal::Loadable const loadables[],
    size_t numLoadables,
    internal::CellFunction cellfun,
    AnyContext const * contextPtr,
    Engine * engine
) :
    AnyArray (Dont::Initialize)
{
    (*cellfun)(this->cell);

    AnyContext context = contextPtr
        ? *contextPtr
        : AnyContext::current(engine);

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
    AnyContext const * contextPtr,
    Engine * engine
) :
    AnyArray (Dont::Initialize)
{
    (*cellfun)(this->cell);


    AnyContext context = contextPtr
        ? *contextPtr
        : AnyContext::current(engine);

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
