#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/words.hpp"
#include "rencpp/context.hpp"

#include "rebol-common.hpp"


namespace ren {

//
// TYPE DETECTION
//

bool Word::isValid(const RenCell & cell) {
    return IS_WORD(AS_C_REBVAL(&cell));
}

bool SetWord::isValid(const RenCell & cell) {
    return IS_SET_WORD(AS_C_REBVAL(&cell));
}

bool GetWord::isValid(const RenCell & cell) {
    return IS_GET_WORD(AS_C_REBVAL(&cell));
}

bool LitWord::isValid(const RenCell & cell) {
    return IS_LIT_WORD(AS_C_REBVAL(&cell));
}

bool Refinement::isValid(const RenCell & cell) {
    return IS_REFINEMENT(AS_C_REBVAL(&cell));
}

bool AnyWord::isValid(const RenCell & cell) {
    return IS_WORD(AS_C_REBVAL(&cell))
        or IS_SET_WORD(AS_C_REBVAL(&cell))
        or IS_GET_WORD(AS_C_REBVAL(&cell))
        or IS_LIT_WORD(AS_C_REBVAL(&cell))
        or IS_REFINEMENT(AS_C_REBVAL(&cell))
        or IS_ISSUE(AS_C_REBVAL(&cell));
}


//
// TYPE HEADER INITIALIZATION
//

void AnyWord::initWord(RenCell & cell) {
    VAL_RESET_HEADER(AS_REBVAL(&cell), REB_WORD);
}

void AnyWord::initSetWord(RenCell & cell) {
    VAL_RESET_HEADER(AS_REBVAL(&cell), REB_SET_WORD);
}

void AnyWord::initGetWord(RenCell & cell) {
    VAL_RESET_HEADER(AS_REBVAL(&cell), REB_GET_WORD);
}

void AnyWord::initLitWord(RenCell & cell) {
    VAL_RESET_HEADER(AS_REBVAL(&cell), REB_LIT_WORD);
}

void AnyWord::initRefinement(RenCell & cell) {
    VAL_RESET_HEADER(AS_REBVAL(&cell), REB_REFINEMENT);
}


//
// SPELLING
//

//
// To get the "formed" version of the word, use to_string.  That will include
// the markup characters, so a GetWord of FOO will give back FOO:
//
// On the other hand, this returns just the "spelling" of the symbol, "FOO"
//

std::string AnyWord::spellingOf_STD() const {
    std::string result = to_string(*this);
    if (is<Word>(*this))
        return result;
    if (is<Refinement>(*this) or is<GetWord>(*this) or is<LitWord>(*this))
        return result.erase(0, 1);
    if (is<SetWord>(*this))
        return result.erase(result.length() - 1, 1);
    throw std::runtime_error {"Invalid Word Type"};
}


#if REN_CLASSLIB_QT
QString AnyWord::spellingOf_QT() const {
    QString result = to_QString(*this);
    if (is<Word>(*this))
        return result;
    if (is<Refinement>(*this) or is<GetWord>(*this) or is<LitWord>(*this))
        return result.right(result.length() - 1);
    if (is<SetWord>(*this))
        return result.left(result.length() - 1);
    throw std::runtime_error {"Invalid Word Type"};
}
#endif



//
// CONSTRUCTION
//

AnyWord::AnyWord (
    char const * spelling,
    internal::CellFunction cellfun,
    AnyContext const * contextPtr,
    Engine * engine
) :
    AnyValue (Dont::Initialize)
{
    (*cellfun)(cell);

    std::string array;

    if (is<Word>(*this)) {
        array += spelling;
    }
    else if (is<SetWord>(*this)) {
        array += spelling;
        array += ':';
    }
    else if (is<GetWord>(*this)) {
        array += ':';
        array += spelling;
    }
    else if (is<LitWord>(*this)) {
        array += '\'';
        array += spelling;
    }
    else if (is<Refinement>(*this)) {
        array += '/';
        array += spelling;
    }
    else
        UNREACHABLE_CODE();

    internal::Loadable loadable = array.data();

    AnyContext context = contextPtr
        ? *contextPtr
        : AnyContext::current(engine);

    constructOrApplyInitialize(
        context.getEngine(),
        &context,
        nullptr, // no applicand
        &loadable,
        1,
        this, // do construct
        nullptr // don't apply
    );

    assert(ANY_WORD(AS_REBVAL(&this->cell)));
}



#if REN_CLASSLIB_QT
AnyWord::AnyWord (
    QString const & spelling,
    internal::CellFunction cellfun,
    AnyContext const * contextPtr,
    Engine * engine
) :
    AnyValue (Dont::Initialize)
{
    (*cellfun)(cell);

    QString source;

    if (is<Word>(*this)) {
        source += spelling;
    }
    else if (is<SetWord>(*this)) {
        source += spelling;
        source += ':';
    }
    else if (is<GetWord>(*this)) {
        source += ':';
        source += spelling;
    }
    else if (is<LitWord>(*this)) {
        source += '\'';
        source += spelling;
    }
    else if (is<Refinement>(*this)) {
        source += '/';
        source += spelling;
    }
    else
        UNREACHABLE_CODE();

    internal::Loadable loadable (source);

    AnyContext context = contextPtr
        ? *contextPtr
        : AnyContext::current(engine);

    constructOrApplyInitialize(
        context.getEngine(),
        &context,
        nullptr, // no applicand
        &loadable,
        1,
        this, // do construct
        nullptr // don't apply
    );

    assert(ANY_WORD(AS_REBVAL(&this->cell)));
}
#endif


AnyWord::AnyWord (AnyWord const & other, internal::CellFunction cellfun) :
    AnyValue (Dont::Initialize)
{
    // !!! There were changes where the header bits started to contain
    // information relevant to the binding.  The original design for an
    // agnostic "cellfun" that would write headers for Rebol-or-Red was
    // such that was the place where the type was encoded.  With the
    // implementations drifting apart, it's likely that this attempt at
    // code sharing won't last.
    //
    // Work around the lost binding issue here with a very temporary hack
    // to write the cell header, then extract the REB_XXX type, then
    // overwrite with the new cell, then put the type bits back.

    (*cellfun)(this->cell);

    enum Reb_Kind kind = VAL_TYPE(AS_REBVAL(&this->cell));
    this->cell = other.cell;
    VAL_SET_TYPE_BITS(AS_REBVAL(&this->cell), kind);

    finishInit(other.origin);
}

} // end namespace ren
