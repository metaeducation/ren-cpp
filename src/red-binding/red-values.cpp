#include "rencpp/values.hpp"

#include "rencpp/red.hpp"

namespace ren {


///
/// INITIALIZATION FINISHER
///

void Value::finishInit(RenEngineHandle engine) {
    if (needsRefcount()) {
        refcountPtr = new RefcountType (1);
    } else {
        refcountPtr = nullptr;
    }

    origin = engine;
}


///
/// VALUE BASE CLASS CONSTRUCTIONS FROM CORRESPONDING C++ TYPES
///

//
// These are provided as a convenience.  The work done in the base class here
// is leveraged by the classes themselves (e.g. when red::Logic wants to
// initialize itself, it calls the red::Value base class initializer for a
// boolean)
//
//     https://github.com/hostilefork/rencpp/issues/2
//


Value::Value (Engine & engine) :
    Value (engine, RedRuntime::makeCell4(RedRuntime::TYPE_UNSET, 0, 0, 0))
{
}


Value::Value (Engine & engine, none_t const &) :
    Value (engine, RedRuntime::makeCell4(RedRuntime::TYPE_NONE, 0, 0, 0))
{
}


Value::Value (Engine & engine, bool const & b) :
    Value (engine, RedRuntime::makeCell4(RedRuntime::TYPE_LOGIC, b, 0, 0))
{
} // not 64-bit aligned, historical accident, TBD before 1.0


Value::Value (Engine & engine, int const & i) :
    Value (engine, RedRuntime::makeCell4(RedRuntime::TYPE_INTEGER, 0, i, 0))
{
} // 64-bit aligned for the someInt value


Value::Value (Engine & engine, double const & d) :
    Value (engine, RedRuntime::makeCell3(RedRuntime::TYPE_FLOAT, 0, d))
{
}



///
/// VALIDMEMFN INSTANCES
///

//
// Rather than passing around an integer ID that may change, this uses the
// member function themselves as the identity of the type.  So the pointer to
// the isSeries member function is the "abstract" Type ID, instead of having
// a separate type.  These can be compared like IDs, but also invoked
// directly.  Many of the functions are willing to take a pointer to a cell
// into which to write the specific bit pattern representing that type.
//

bool Value::isUnset() const {
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_UNSET;
}


bool Value::isNone() const {
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_NONE;
}


bool Value::isLogic() const {
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_LOGIC;
}


bool Value::isTrue() const {
    return isLogic() && cell.data1;
}


bool Value::isFalse() const {
    return isLogic() && !cell.data1;
}


bool Value::isInteger() const {
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_INTEGER;
}


bool Value::isFloat() const {
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_FLOAT;
}


bool Value::isWord(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_WORD;
        return true;
    }
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_WORD;
}


bool Value::isSetWord(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_SET_WORD;
        return true;
    }
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_SET_WORD;
}


bool Value::isGetWord(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_GET_WORD;
        return true;
    }
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_GET_WORD;
}


bool Value::isLitWord(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_LIT_WORD;
        return true;
    }
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_LIT_WORD;
}


bool Value::isRefinement(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_REFINEMENT;
        return true;
    }
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_REFINEMENT;
}


bool Value::isIssue(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_ISSUE;
        return true;
    }
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_ISSUE;
}


bool Value::isAnyWord() const {
    switch (RedRuntime::getDatatypeID(*this)) {
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


bool Value::isBlock(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_BLOCK;
        return true;
    }
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_BLOCK;
}


bool Value::isParen(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_PAREN;
        return true;
    }
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_PAREN;
}


bool Value::isPath(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_PATH;
        return true;
    }
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_PATH;
}


bool Value::isGetPath(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_GET_PATH;
        return true;
    }
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_GET_PATH;
}


bool Value::isSetPath(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_SET_PATH;
        return true;
    }
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_SET_PATH;
}


bool Value::isLitPath(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_LIT_PATH;
        return true;
    }
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_LIT_PATH;
}


bool Value::isAnyBlock() const {
    switch (RedRuntime::getDatatypeID(*this)) {
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

bool Value::isAnyString() const {
    switch (RedRuntime::getDatatypeID(*this)) {
        case RedRuntime::TYPE_STRING:
        case RedRuntime::TYPE_FILE:
        case RedRuntime::TYPE_URL:
            return true;
        default:
            break;
    }
    return false;
}


bool Value::isSeries() const {
    return isAnyBlock() || isAnyString();
}


bool Value::isString(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_STRING;
        return true;
    }
    return RedRuntime::getDatatypeID(*this) == RedRuntime::TYPE_STRING;
}



///
/// ADDITIONAL CLASS SUPPORT
///

//
// This is where any other bit formatting is done that is specific to the
// Red runtime.  It can take advantage of any efficiency it likes, or defer
// to the ConstructOrApply hook to call into the binding.
//

None::None (Engine & engine) :
    Value (engine, RedRuntime::makeCell4(RedRuntime::TYPE_NONE, 0, 0, 0))
{
}


Integer::operator int () const {
    return cell.s.data2;
}



///
/// FUNCTION FINALIZER FOR EXTENSION
///

//
// This is a work in progress on the Rebol branch, as Extensions and such are
// even more bleeding edge than the rest...but this would be how a RedCell
// would stow a pointer into a function if it worked.
//

void Function::finishInit(
    Engine & engine,
    Block const & spec,
    RenShimPointer const & shim
) {
    throw std::runtime_error("No way to make RedCell from C++ function yet.");

    UNUSED(spec);
    UNUSED(shim);

    Value::finishInit(engine.getHandle());
}

} // end namespace ren
