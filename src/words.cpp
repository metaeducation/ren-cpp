#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/words.hpp"
#include "rencpp/context.hpp"

#include "common.hpp"


namespace ren {

//
// TYPE DETECTION
//

bool Word::isValid(const REBVAL *cell) {
    return IS_WORD(cell);
}

bool SetWord::isValid(const REBVAL *cell) {
    return IS_SET_WORD(cell);
}

bool GetWord::isValid(const REBVAL *cell) {
    return IS_GET_WORD(cell);
}

bool LitWord::isValid(const REBVAL *cell) {
    return IS_LIT_WORD(cell);
}

bool Refinement::isValid(const REBVAL *cell) {
    return IS_REFINEMENT(cell);
}

bool AnyWord::isValid(const REBVAL *cell) {
    return IS_WORD(cell)
        || IS_SET_WORD(cell)
        || IS_GET_WORD(cell)
        || IS_LIT_WORD(cell)
        || IS_REFINEMENT(cell)
        || IS_ISSUE(cell);
}


//
// TYPE HEADER INITIALIZATION
//

void AnyWord::initWord(REBVAL *cell) {
    VAL_RESET_HEADER(cell, REB_WORD);
}

void AnyWord::initSetWord(REBVAL *cell) {
    VAL_RESET_HEADER(cell, REB_SET_WORD);
}

void AnyWord::initGetWord(REBVAL *cell) {
    VAL_RESET_HEADER(cell, REB_GET_WORD);
}

void AnyWord::initLitWord(REBVAL *cell) {
    VAL_RESET_HEADER(cell, REB_LIT_WORD);
}

void AnyWord::initRefinement(REBVAL *cell) {
    VAL_RESET_HEADER(cell, REB_REFINEMENT);
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
    if (hasType<Word>(*this))
        return result;
    if (
        hasType<Refinement>(*this)
        || hasType<GetWord>(*this)
        || hasType<LitWord>(*this)
    ){
        return result.erase(0, 1);
    }
    if (hasType<SetWord>(*this))
        return result.erase(result.length() - 1, 1);
    throw std::runtime_error {"Invalid Word Type"};
}


#if REN_CLASSLIB_QT
QString AnyWord::spellingOf_QT() const {
    QString result = to_QString(*this);
    if (hasType<Word>(*this))
        return result;
    if (
        hasType<Refinement>(*this)
        || hasType<GetWord>(*this)
        || hasType<LitWord>(*this)
    ){
        return result.right(result.length() - 1);
    }
    if (hasType<SetWord>(*this))
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

    if (hasType<Word>(*this)) {
        array += spelling;
    }
    else if (hasType<SetWord>(*this)) {
        array += spelling;
        array += ':';
    }
    else if (hasType<GetWord>(*this)) {
        array += ':';
        array += spelling;
    }
    else if (hasType<LitWord>(*this)) {
        array += '\'';
        array += spelling;
    }
    else if (hasType<Refinement>(*this)) {
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

    assert(ANY_WORD(this->cell));
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

    if (hasType<Word>(*this)) {
        source += spelling;
    }
    else if (hasType<SetWord>(*this)) {
        source += spelling;
        source += ':';
    }
    else if (hasType<GetWord>(*this)) {
        source += ':';
        source += spelling;
    }
    else if (hasType<LitWord>(*this)) {
        source += '\'';
        source += spelling;
    }
    else if (hasType<Refinement>(*this)) {
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

    assert(ANY_WORD(this->cell));
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

    enum Reb_Kind kind = VAL_TYPE(this->cell);
    RL_Move(this->cell, other.cell);
    VAL_SET_TYPE_BITS(this->cell, kind);

    finishInit(other.origin);
}

} // end namespace ren
