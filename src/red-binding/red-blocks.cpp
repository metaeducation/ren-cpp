#include "rencpp/values.hpp"
#include "rencpp/blocks.hpp"

#include "rencpp/red.hpp"


#define UNUSED(x) static_cast<void>(x)


namespace ren {

///
/// TYPE CHECKING AND INITIALIZATION
///

bool Value::isBlock(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_BLOCK;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_BLOCK;
}


bool Value::isParen(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_PAREN;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_PAREN;
}


bool Value::isPath(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_PATH;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_PATH;
}


bool Value::isGetPath(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_GET_PATH;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_GET_PATH;
}


bool Value::isSetPath(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_SET_PATH;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_SET_PATH;
}


bool Value::isLitPath(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_LIT_PATH;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_LIT_PATH;
}


bool Value::isAnyBlock() const {
    switch (RedRuntime::getDatatypeID(this->cell)) {
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



///
/// CONSTRUCTION
///

AnyBlock::AnyBlock (
    internal::Loadable const loadables[],
    size_t numLoadables,
    internal::CellFunction cellfun,
    Context const * contextPtr,
    Engine * engine
) :
    Series (Dont::Initialize)
{
    throw std::runtime_error("AnyBlock::AnyBlock coming soon...");

    UNUSED(loadables);
    UNUSED(numLoadables);
    UNUSED(cellfun);
    UNUSED(contextPtr);
    UNUSED(engine);
}


} // end namespace ren
