#include "rencpp/value.hpp"
#include "rencpp/words.hpp"

#include "rencpp/red.hpp"


#define UNUSED(x) static_cast<void>(x)

namespace ren {


//
// TYPE CHECKING
//

bool Word::isValid(RenCell const * cell) {
    return RedRuntime::getDatatypeID(cell) == RedRuntime::TYPE_WORD;
}


bool SetWord::isValid(RenCell const * cell) {
    return RedRuntime::getDatatypeID(cell) == RedRuntime::TYPE_SET_WORD;
}


bool GetWord::isValid(RenCell const * cell) {
    return RedRuntime::getDatatypeID(cell) == RedRuntime::TYPE_GET_WORD;
}


bool LitWord::isValid(RenCell const * cell) {
    return RedRuntime::getDatatypeID(cell) == RedRuntime::TYPE_LIT_WORD;
}


bool Refinement::isValid(RenCell const * cell) {
    return RedRuntime::getDatatypeID(cell) == RedRuntime::TYPE_REFINEMENT;
}


bool AnyWord::isValid(RenCell const * cell) {
    switch (RedRuntime::getDatatypeID(cell)) {
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
// TYPE HEADER INITIALIZATION
//

void AnyWord::initWord(RedCell & cell) {
    cell.header = RedRuntime::TYPE_WORD;
}


void AnyWord::initSetWord(RedCell & cell) {
    cell.header = RedRuntime::TYPE_SET_WORD;
}


void AnyWord::initGetWord(RedCell & cell) {
    cell.header = RedRuntime::TYPE_GET_WORD;
}


void AnyWord::initLitWord(RedCell & cell) {
    cell.header = RedRuntime::TYPE_LIT_WORD;
}


void AnyWord::initRefinement(RedCell & cell) {
    cell.header = RedRuntime::TYPE_REFINEMENT;
}



//
// CONSTRUCTION
//

AnyWord::AnyWord (
    char const * cstr,
    internal::CellFunction cellfun,
    AnyContext const * context,
    Engine * engine
) {
    throw std::runtime_error("AnyWord::AnyWord coming soon...");

    UNUSED(cstr);
    UNUSED(cellfun);
    UNUSED(context);
    UNUSED(engine);
}


} // end namespace ren
