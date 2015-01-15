#include <stdexcept>

#include "rencpp/values.hpp"
#include "rencpp/context.hpp"
#include "rencpp/engine.hpp"

#include "rencpp/rebol.hpp"

namespace ren {

bool Value::isEqualTo(Value const & other) const {
    return Compare_Values(
        const_cast<REBVAL *>(&cell),
        const_cast<REBVAL *>(&other.cell),
        0 // REBNATIVE(equalq)
    );
}

bool Value::isSameAs(Value const & other) const {
    return Compare_Values(
        const_cast<REBVAL *>(&cell),
        const_cast<REBVAL *>(&other.cell),
        3 // REBNATIVE(sameq)
    );
}


Value::Value (unset_t const &, Engine * engine) :
    Value (Dont::Initialize)
{
    SET_UNSET(&cell);
    finishInit(engine);
}

Value::Value (none_t const &, Engine * engine) :
    Value (Dont::Initialize)
{
    SET_NONE(&cell);
    finishInit(engine);
}

Value::Value (bool const & someBool, Engine * engine) :
    Value (Dont::Initialize)
{
    SET_LOGIC(&cell, someBool);
    finishInit(engine);
}

Value::Value (char const & c, Engine * engine) :
    Value (Dont::Initialize)
{
    SET_CHAR(&cell, c);
    finishInit(engine);
}

Value::Value (wchar_t const & wc, Engine * engine) :
    Value (Dont::Initialize)
{
    SET_CHAR(&cell, wc);
    finishInit(engine);
}

Value::Value (int const & someInt, Engine * engine) :
    Value (Dont::Initialize)
{
    SET_INTEGER(&cell, someInt);
    finishInit(engine);
}

Value::Value (double const & someDouble, Engine * engine) :
    Value (Dont::Initialize)
{
    SET_DECIMAL(&cell, someDouble);
    finishInit(engine);
}

//
// The only way the client can get handles of types that need some kind of
// garbage collection participation right now is if the system gives it to
// them.  So on the C++ side, they never create series (for instance).  It's
// a good check of the C++ wrapper to do some reference counting of the
// series that have been handed back.
//

void Value::finishInit(RenEngineHandle engine) {
    if (needsRefcount()) {
        refcountPtr = new RefcountType (1);

    #ifndef NDEBUG
        auto it = internal::nodes[engine.data].find(VAL_SERIES(&cell));
        if (it == internal::nodes[engine.data].end())
            internal::nodes[engine.data].emplace(VAL_SERIES(&cell), 1);
        else
            it->second++;
    #endif

    } else {
        refcountPtr = nullptr;
    }

    origin = engine;
}


void Value::finishInit(Engine * engine) {
    if (not engine)
        engine = &Engine::runFinder();
    finishInit(engine->getHandle());
}



///
/// TYPE DETECTORS AND FILLERS
///

bool Value::isUnset() const {
    return IS_UNSET(&cell);
}

bool Value::isNone() const {
    return IS_NONE(&cell);
}

bool Value::isLogic() const {
    return IS_LOGIC(&cell);
}

bool Value::isTrue() const {
    return isLogic() && VAL_LOGIC(&cell);
}

bool Value::isFalse() const {
    return isLogic() && !VAL_LOGIC(&cell);
}

bool Value::isCharacter() const {
    return IS_CHAR(&cell);
}

bool Value::isInteger() const {
    return IS_INTEGER(&cell);
}

bool Value::isFloat() const {
    return IS_DECIMAL(&cell);
}

bool Value::isDate() const {
    return IS_DATE(&cell);
}

bool Value::isWord(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_WORD);
        return true;
    }
    return IS_WORD(&cell);
}

bool Value::isSetWord(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_SET_WORD);
        return true;
    }
    return IS_SET_WORD(&cell);
}

bool Value::isGetWord(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_GET_WORD);
        return true;
    }
    return IS_GET_WORD(&cell);
}

bool Value::isLitWord(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_LIT_WORD);
        return true;
    }
    return IS_LIT_WORD(&cell);
}

bool Value::isRefinement(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_REFINEMENT);
        return true;
    }
    return IS_REFINEMENT(&cell);
}

bool Value::isIssue(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_ISSUE);
        return true;
    }
    return IS_ISSUE(&cell);
}

bool Value::isAnyWord() const {
    return IS_WORD(&cell) or IS_SET_WORD(&cell) or IS_GET_WORD(&cell)
        or IS_LIT_WORD(&cell) or IS_REFINEMENT(&cell) or IS_ISSUE(&cell);
}

bool Value::isBlock(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_BLOCK);
        return true;
    }
    return IS_BLOCK(&cell);
}

bool Value::isParen(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_PAREN);
        return true;
    }
    return IS_PAREN(&cell);
}

bool Value::isPath(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_PATH);
        return true;
    }
    return IS_PATH(&cell);
}

bool Value::isGetPath(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_GET_PATH);
        return true;
    }
    return IS_GET_PATH(&cell);
}

bool Value::isSetPath(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_SET_PATH);
        return true;
    }
    return IS_SET_PATH(&cell);
}

bool Value::isLitPath(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_LIT_PATH);
        return true;
    }
    return IS_LIT_PATH(&cell);
}

bool Value::isAnyBlock() const {
    return IS_BLOCK(&cell) or IS_PAREN(&cell) or IS_PATH(&cell)
        or IS_SET_PATH(&cell) or IS_GET_PATH(&cell) or IS_LIT_PATH(&cell);
}

bool Value::isAnyString() const {
    return IS_STRING(&cell) or IS_TAG(&cell)
        or IS_FILE(&cell) or IS_URL(&cell);
}

bool Value::isSeries() const {
    return isAnyBlock() or isAnyString() /* or isBinary()*/;
}

bool Value::isString(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_STRING);
        return true;
    }
    return IS_STRING(&cell);
}

bool Value::isTag(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_TAG);
        return true;
    }
    return IS_TAG(&cell);
}

bool Value::isFilename(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_FILE);
        return true;
    }
    return IS_FILE(&cell);
}


bool Value::isFunction() const {
    // Really, from a user's point of view...shouldn't there only be
    // ANY_FUNCTION?  It's currently annoying if someone checks for
    // taking a function and rejects closure.

    return IS_FUNCTION(&cell)
        or IS_NATIVE(&cell)
        or IS_CLOSURE(&cell)
        or IS_ACTION(&cell);
}

bool Value::isContext() const {
    return IS_OBJECT(&cell);
}

bool Value::isError() const {
    return IS_ERROR(&cell);
}


/////////////////////////////////////////////////////////////////////////////

Logic::operator bool() const {
    return VAL_INT32(&cell);
}

Integer::operator int() const {
    return VAL_INT32(&cell);
}

Float::operator double() const {
    return VAL_DECIMAL(&cell);
}


Character::operator char () const {
    REBUNI uni = VAL_CHAR(&cell);
    if (uni > 127)
        throw std::runtime_error("Non-ASCII codepoint cast to char");
    return static_cast<char>(uni);
}


Character::operator wchar_t () const {
    REBUNI uni = VAL_CHAR(&cell);
    // will throw in Red for "astral plane" unicode codepoints
    return static_cast<wchar_t>(uni);
}


long Character::codepoint() const {
    REBUNI uni = VAL_CHAR(&cell);
    // will probably not throw in Red, either
    return uni;
}


#if REN_CLASSLIB_QT
Character::operator QChar () const {
    REBUNI uni = VAL_CHAR(&cell);
    return QChar(uni);
}


AnyString::AnyString (
    QString const & spelling,
    internal::CellFunction cellfun,
    Engine * engine
)
    : Series(Dont::Initialize)
{
    (this->*cellfun)(&this->cell);

    QByteArray array;

    // Note: wouldn't be able to return char * without intermediate
    // http://stackoverflow.com/questions/17936160/

    if (isString()) {
        array += '{';
        array += spelling.toLocal8Bit();
        array += '}';
    }
    else if (isTag()) {
        array += '<';
        array += spelling.toLocal8Bit();
        array += '>';
    }
    else
        UNREACHABLE_CODE();

    internal::Loadable loadable = array.data();

    if (not engine)
        engine = &Engine::runFinder();

    constructOrApplyInitialize(
        engine->getHandle(),
        nullptr, // no context
        nullptr, // no applicand
        &loadable,
        1,
        this, // do construct
        nullptr // don't apply
    );
}
#endif

#if REN_CLASSLIB_STD
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
#endif


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



#if REN_CLASSLIB_STD
std::string AnyString::spellingOf_STD() const {
    std::string result = static_cast<std::string>(*this);
    if (isString() /* or isUrl() or isEmail() or isFile() */)
        return result;
    if (isTag()) {
        result.erase(0, 1);
        return result.erase(result.length() - 1, 1);
    }
    throw std::runtime_error {"Invalid String Type"};
}
#endif


#if REN_CLASSLIB_QT
QString AnyString::spellingOf_QT() const {
    QString result = static_cast<QString>(*this);
    if (isString() /* or isUrl() or isEmail() or isFile() */)
        return result;
    if (isTag()) {
        assert(result.length() >= 2);
        return result.mid(1, result.length() - 2);
    }
    throw std::runtime_error {"Invalid String Type"};
}
#endif



///
/// ITERATORS (WORK IN PROGRESS)
///

void ren::internal::Series_::operator++() {
    cell.data.series.index++;
}

void ren::internal::Series_::operator--() {
    cell.data.series.index--;
}

void ren::internal::Series_::operator++(int) {
    ++*this;
}

void ren::internal::Series_::operator--(int) {
    --*this;
}

Value ren::internal::Series_::operator*() const {
    Value result {Dont::Initialize};

    if (isAnyString()) {
        // from str_to_char in Rebol source
        SET_CHAR(
            &result.cell,
            GET_ANY_CHAR(VAL_SERIES(&cell), cell.data.series.index)
        );
    } else if (isAnyBlock()) {
        result.cell = *VAL_BLK_SKIP(&cell, cell.data.series.index);
    } else {
        // Binary and such, would return an integer
        UNREACHABLE_CODE();
    }
    result.finishInit(origin);
    return result;
}

Value ren::internal::Series_::operator->() const {
    return *(*this);
}

void ren::internal::Series_::head() {
    cell.data.series.index = static_cast<REBCNT>(0);
}

void ren::internal::Series_::tail() {
    cell.data.series.index
        = static_cast<REBCNT>(cell.data.series.series->tail);
}

size_t Series::length() const {
    REBINT index = (REBINT)VAL_INDEX(&cell);
    REBINT tail = (REBINT)VAL_TAIL(&cell);
    return tail > index ? tail - index : 0;
}



Error::Error (const char * msg, Engine * engine) :
    Value (Dont::Initialize)
{
    VAL_SET(&this->cell, REB_ERROR);

    if (not engine)
        engine = &Engine::runFinder();

    std::string array {"#[error! [code: 800 type: 'User id: 'message arg1: "};

    array += '"';
    array += msg;
    array += '"';

    // the shim could adjust the where and say what function threw it?
    // file/line number optional?

    array += " arg2: none arg3: none near: {ren::Error} where: none]]";

    internal::Loadable loadable = array.data();

    constructOrApplyInitialize(
        engine->getHandle(),
        nullptr, // no context
        nullptr, // no applicand
        &loadable,
        1,
        this, // Do construct
        nullptr // Don't apply
    );
}



} // end namespace ren
