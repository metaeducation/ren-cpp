#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/arrays.hpp"
#include "rencpp/context.hpp"

#include "rencpp/rebol.hpp"

namespace ren {

//
// TYPE DETECTION
//

bool Block::isValid(RenCell const & cell) {
    return IS_BLOCK(&cell);
}

bool Group::isValid(RenCell const & cell) {
    return IS_PAREN(&cell);
}

bool Path::isValid(RenCell const & cell) {
    return IS_PATH(&cell);
}

bool GetPath::isValid(RenCell const & cell) {
    return IS_GET_PATH(&cell);
}

bool SetPath::isValid(RenCell const & cell) {
    return IS_SET_PATH(&cell);
}

bool LitPath::isValid(RenCell const & cell) {
    return IS_LIT_PATH(&cell);
}

bool AnyArray::isValid(RenCell const & cell) {
    return IS_BLOCK(&cell) or IS_PAREN(&cell) or IS_PATH(&cell)
        or IS_SET_PATH(&cell) or IS_GET_PATH(&cell) or IS_LIT_PATH(&cell);
}


//
// TYPE HEADER INITIALIZATION
//

void AnyArray::initBlock(RenCell & cell) {
    VAL_SET(&cell, REB_BLOCK);
}

void AnyArray::initGroup(RenCell & cell) {
    VAL_SET(&cell, REB_PAREN);
}

void AnyArray::initPath(RenCell & cell) {
    VAL_SET(&cell, REB_PATH);
}

void AnyArray::initGetPath(RenCell & cell) {
    VAL_SET(&cell, REB_GET_PATH);
}

void AnyArray::initSetPath(RenCell & cell) {
    VAL_SET(&cell, REB_SET_PATH);
}

void AnyArray::initLitPath(RenCell & cell) {
    VAL_SET(&cell, REB_LIT_PATH);
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
    (*cellfun)(this->cell);

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
    (*cellfun)(this->cell);


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
