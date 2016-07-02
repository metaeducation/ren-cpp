#include "rencpp/value.hpp"
#include "rencpp/arrays.hpp"

#include "rencpp/red.hpp"


#define UNUSED(x) static_cast<void>(x)


namespace ren {

//
// TYPE CHECKING
//

bool Block::isValid(RenCell const * cell) {
    return RedRuntime::getDatatypeID(cell) == RedRuntime::TYPE_BLOCK;
}


bool Group::isValid(RenCell const * cell) {
    return RedRuntime::getDatatypeID(cell) == RedRuntime::TYPE_PAREN;
}


bool Path::isValid(RenCell const * cell) {
    return RedRuntime::getDatatypeID(cell) == RedRuntime::TYPE_PATH;
}


bool GetPath::isValid(RenCell const * cell) {
    return RedRuntime::getDatatypeID(cell) == RedRuntime::TYPE_GET_PATH;
}


bool SetPath::isValid(RenCell const * cell) {
    return RedRuntime::getDatatypeID(cell) == RedRuntime::TYPE_SET_PATH;
}


bool LitPath::isValid(RenCell const * cell) {
    return RedRuntime::getDatatypeID(cell) == RedRuntime::TYPE_LIT_PATH;
}


bool AnyArray::isValid(RenCell const * cell) {
    switch (RedRuntime::getDatatypeID(cell)) {
        case RedRuntime::TYPE_BLOCK:
        case RedRuntime::TYPE_PAREN:
        case RedRuntime::TYPE_PATH:
        case RedRuntime::TYPE_SET_PATH:
        case RedRuntime::TYPE_GET_PATH:
        case RedRuntime::TYPE_LIT_PATH:
            return true;
        default:
            break;
    }
    return false;
}


//
// TYPE HEADER INITIALIZATION
//

void AnyArray::initBlock(RedCell & cell) {
    cell.header = RedRuntime::TYPE_BLOCK;
}


void AnyArray::initGroup(RedCell & cell) {
    cell.header = RedRuntime::TYPE_PAREN;
}


void AnyArray::initPath(RedCell & cell) {
    cell.header = RedRuntime::TYPE_PATH;
}


void AnyArray::initGetPath(RedCell & cell) {
    cell.header = RedRuntime::TYPE_GET_PATH;
}


void AnyArray::initSetPath(RedCell & cell) {
    cell.header = RedRuntime::TYPE_SET_PATH;
}


void AnyArray::initLitPath(RedCell & cell) {
    cell.header = RedRuntime::TYPE_LIT_PATH;
}



///
/// CONSTRUCTION
///

AnyArray::AnyArray (
    internal::Loadable const loadables[],
    size_t numLoadables,
    internal::CellFunction cellfun,
    AnyContext const * contextPtr,
    Engine * engine
) :
    AnySeries (Dont::Initialize)
{
    throw std::runtime_error("AnyArray::AnyArray coming soon...");

    UNUSED(loadables);
    UNUSED(numLoadables);
    UNUSED(cellfun);
    UNUSED(contextPtr);
    UNUSED(engine);
}


} // end namespace ren
