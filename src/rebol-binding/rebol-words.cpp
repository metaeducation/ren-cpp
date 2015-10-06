#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/words.hpp"
#include "rencpp/context.hpp"


namespace ren {

//
// TYPE DETECTION AND INITIALIZATION
//

bool AnyValue::isWord(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_WORD);
        return true;
    }
    return IS_WORD(&cell);
}

bool AnyValue::isSetWord(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_SET_WORD);
        return true;
    }
    return IS_SET_WORD(&cell);
}

bool AnyValue::isGetWord(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_GET_WORD);
        return true;
    }
    return IS_GET_WORD(&cell);
}

bool AnyValue::isLitWord(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_LIT_WORD);
        return true;
    }
    return IS_LIT_WORD(&cell);
}

bool AnyValue::isRefinement(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_REFINEMENT);
        return true;
    }
    return IS_REFINEMENT(&cell);
}

bool AnyValue::isIssue(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_ISSUE);
        return true;
    }
    return IS_ISSUE(&cell);
}

bool AnyValue::isAnyWord() const {
    return IS_WORD(&cell)
        or IS_SET_WORD(&cell)
        or IS_GET_WORD(&cell)
        or IS_LIT_WORD(&cell)
        or IS_REFINEMENT(&cell)
        or IS_ISSUE(&cell);
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
    if (isWord())
        return result;
    if (isRefinement() or isGetWord() or isLitWord() or isIssue())
        return result.erase(0, 1);
    if (isSetWord())
        return result.erase(result.length() - 1, 1);
    throw std::runtime_error {"Invalid Word Type"};
}


#if REN_CLASSLIB_QT
QString AnyWord::spellingOf_QT() const {
    QString result = to_QString(*this);
    if (isWord())
        return result;
    if (isRefinement() or isGetWord() or isLitWord() or isIssue())
        return result.right(result.length() - 1);
    if (isSetWord())
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
    Context const * contextPtr,
    Engine * engine
) :
    AnyValue (Dont::Initialize)
{
    (this->*cellfun)(&this->cell);

    std::string array;

    if (isWord()) {
        array += spelling;
    }
    else if (isSetWord()) {
        array += spelling;
        array += ':';
    }
    else if (isGetWord()) {
        array += ':';
        array += spelling;
    }
    else if (isLitWord()) {
        array += '\'';
        array += spelling;
    }
    else if (isRefinement()) {
        array += '/';
        array += spelling;
    }
    else
        UNREACHABLE_CODE();

    internal::Loadable loadable = array.data();

    Context context = contextPtr ? *contextPtr : Context::current(engine);

    constructOrApplyInitialize(
        context.getEngine(),
        &context,
        nullptr, // no applicand
        &loadable,
        1,
        this, // do construct
        nullptr // don't apply
    );

    VAL_WORD_FRAME(&this->cell) = VAL_OBJ_FRAME(&context.cell);
}



#if REN_CLASSLIB_QT
AnyWord::AnyWord (
    QString const & spelling,
    internal::CellFunction cellfun,
    Context const * contextPtr,
    Engine * engine
) :
    AnyValue (Dont::Initialize)
{
    (this->*cellfun)(&this->cell);

    QString source;

    if (isWord()) {
        source += spelling;
    }
    else if (isSetWord()) {
        source += spelling;
        source += ':';
    }
    else if (isGetWord()) {
        source += ':';
        source += spelling;
    }
    else if (isLitWord()) {
        source += '\'';
        source += spelling;
    }
    else if (isRefinement()) {
        source += '/';
        source += spelling;
    }
    else
        UNREACHABLE_CODE();

    internal::Loadable loadable (source);

    Context context = contextPtr ? *contextPtr : Context::current(engine);

    constructOrApplyInitialize(
        context.getEngine(),
        &context,
        nullptr, // no applicand
        &loadable,
        1,
        this, // do construct
        nullptr // don't apply
    );

    VAL_WORD_FRAME(&this->cell) = VAL_OBJ_FRAME(&context.cell);
}
#endif


AnyWord::AnyWord (AnyWord const & other, internal::CellFunction cellfun) :
    AnyValue (Dont::Initialize)
{
    this->cell = other.cell;
    (this->*cellfun)(&this->cell);
    finishInit(other.origin);
}

} // end namespace ren
