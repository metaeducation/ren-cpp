#include "rencpp/value.hpp"
#include "rencpp/words.hpp"

#include "rencpp/red.hpp"


#define UNUSED(x) static_cast<void>(x)

namespace ren {


///
/// TYPE CHECKING AND INITIALIZATION
///

bool AnyValue::isWord(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_WORD;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_WORD;
}


bool AnyValue::isSetWord(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_SET_WORD;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_SET_WORD;
}


bool AnyValue::isGetWord(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_GET_WORD;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_GET_WORD;
}


bool AnyValue::isLitWord(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_LIT_WORD;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_LIT_WORD;
}


bool AnyValue::isRefinement(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_REFINEMENT;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_REFINEMENT;
}


bool AnyValue::isIssue(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_ISSUE;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_ISSUE;
}


bool AnyValue::isAnyWord() const {
    switch (RedRuntime::getDatatypeID(this->cell)) {
        case RedRuntime::TYPE_WORD:
        case RedRuntime::TYPE_SET_WORD:
        case RedRuntime::TYPE_GET_WORD:
        case RedRuntime::TYPE_LIT_WORD:
        case RedRuntime::TYPE_REFINEMENT:
        case RedRuntime::TYPE_ISSUE:
            return true;
        default:
            break;
    }
    return false;
}


//
// CONSTRUCTION
//

AnyWord::AnyWord (
    char const * cstr,
    internal::CellFunction cellfun,
    Context const * context,
    Engine * engine
) {
    throw std::runtime_error("AnyWord::AnyWord coming soon...");

    UNUSED(cstr);
    UNUSED(cellfun);
    UNUSED(context);
    UNUSED(engine);
}


} // end namespace ren
