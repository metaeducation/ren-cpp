#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/arrays.hpp"
#include "rencpp/context.hpp"

#include "rencpp/rebol.hpp"

#include "common.hpp"


namespace ren {

//
// TYPE DETECTION
//

bool Block::isValid(REBVAL const * cell) {
    return IS_BLOCK(cell);
}

bool Group::isValid(REBVAL const * cell) {
    return IS_GROUP(cell);
}

bool Path::isValid(REBVAL const * cell) {
    return IS_PATH(cell);
}

bool GetPath::isValid(REBVAL const * cell) {
    return IS_GET_PATH(cell);
}

bool SetPath::isValid(REBVAL const * cell) {
    return IS_SET_PATH(cell);
}

bool LitPath::isValid(REBVAL const * cell) {
    return IS_LIT_PATH(cell);
}

bool AnyArray::isValid(REBVAL const * cell) {
    return IS_BLOCK(cell)
        || IS_GROUP(cell)
        || IS_PATH(cell)
        || IS_SET_PATH(cell)
        || IS_GET_PATH(cell)
        || IS_LIT_PATH(cell);
}


//
// TYPE HEADER INITIALIZATION
//

void AnyArray::initBlock(REBVAL *cell) {
    VAL_RESET_HEADER(cell, REB_BLOCK);
}

void AnyArray::initGroup(REBVAL *cell) {
    VAL_RESET_HEADER(cell, REB_GROUP);
}

void AnyArray::initPath(REBVAL *cell) {
    VAL_RESET_HEADER(cell, REB_PATH);
}

void AnyArray::initGetPath(REBVAL *cell) {
    VAL_RESET_HEADER(cell, REB_GET_PATH);
}

void AnyArray::initSetPath(REBVAL *cell) {
    VAL_RESET_HEADER(cell, REB_SET_PATH);
}

void AnyArray::initLitPath(REBVAL *cell) {
    VAL_RESET_HEADER(cell, REB_LIT_PATH);
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
