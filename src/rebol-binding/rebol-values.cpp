#include "rencpp/values.hpp"
#include "rencpp/context.hpp"

#include "rencpp/rebol.hpp"

namespace ren {


Value::Value (Engine & engine) :
    Value (Dont::Initialize)
{
    SET_UNSET(&cell);
    finishInit(engine.getHandle());
}

Value::Value (Engine & engine, none_t const &) :
    Value (Dont::Initialize)
{
    SET_NONE(&cell);
    finishInit(engine.getHandle());
}

Value::Value (Engine & engine, bool const & someBool) :
    Value (Dont::Initialize)
{
    SET_LOGIC(&cell, someBool);
    finishInit(engine.getHandle());
}


Value::Value (Engine & engine, int const & someInt) :
    Value (Dont::Initialize)
{
    SET_INTEGER(&cell, someInt);
    finishInit(engine.getHandle());
}

Value::Value (Engine & engine, double const & someDouble) :
    Value (Dont::Initialize)
{
    SET_DECIMAL(&cell, someDouble);
    finishInit(engine.getHandle());
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
            (*it).second++;
    #endif

    } else {
        refcountPtr = nullptr;
    }

    origin = engine;
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

bool Value::isFunction() const {
    return IS_FUNCTION(&cell);
}

//////////////////////////////////////////////////////////////////////////////

None::None (Engine & engine) :
    Value (Dont::Initialize)
{
    SET_NONE(&cell);
    finishInit(engine.getHandle());
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


#ifdef REN_CLASSLIB_STD
std::string AnyWord::spellingOf_STD() const {
    std::string result = static_cast<std::string>(*this);
    if (isWord())
        return result;
    if (isRefinement() or isGetWord() or isIssue())
        return result.erase(0, 1);
    if (isSetWord())
        return result.erase(result.length() - 1, 1);
    throw std::runtime_error {"Invalid Word Type"};
}
#endif


#ifdef REN_CLASSLIB_QT
QString AnyWord::spellingOf_QT() const {
    QString result = static_cast<QString>(*this);
    if (isWord())
        return result;
    if (isRefinement() or isGetWord() or isIssue())
        return result.right(result.length() - 1);
    if (isSetWord())
        return result.left(result.length() - 1);
    throw std::runtime_error {"Invalid Word Type"};
}
#endif




///
/// FUNCTION FINALIZER FOR EXTENSION
///

void Function::finishInit(
    Engine & engine,
    Block const & spec,
    RenShimPointer const & shim
) {
    Make_Native(&cell, VAL_SERIES(&spec.cell), shim, REB_NATIVE);

    Value::finishInit(engine.getHandle());
}

} // end namespace ren
