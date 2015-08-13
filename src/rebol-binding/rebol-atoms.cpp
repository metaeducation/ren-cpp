#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/atoms.hpp"
#include "rencpp/engine.hpp"


namespace ren {

bool Value::isAtom() const {
    // Will be more efficient when atom makes it formally into the
    // Rebol base typesets.
    return (
        IS_UNSET(&cell)
        || IS_NONE(&cell)
        || IS_LOGIC(&cell)
        || IS_CHAR(&cell)
        || IS_INTEGER(&cell)
        || IS_DECIMAL(&cell)
        || IS_DATE(&cell)
    );
}


//
// UNSET
//

bool Value::isUnset() const {
    return IS_UNSET(&cell);
}

Value::Value (unset_t, Engine * engine) noexcept :
    Value (Dont::Initialize)
{
    SET_UNSET(&cell);

    // !!! Should some types not need an engine field?
    if (not engine)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}



//
// NONE
//

bool Value::isNone() const {
    return IS_NONE(&cell);
}

Value::Value (none_t, Engine * engine) noexcept :
    Value (Dont::Initialize)
{
    SET_NONE(&cell);

    // !!! Should some types not need an engine field?
    if (not engine)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}



//
// LOGIC
//

bool Value::isLogic() const {
    return IS_LOGIC(&cell);
}

bool Value::isTrue() const {
    return isLogic() && VAL_LOGIC(&cell);
}

bool Value::isFalse() const {
    return isLogic() && !VAL_LOGIC(&cell);
}

Value::Value (bool someBool, Engine * engine) noexcept :
    Value (Dont::Initialize)
{
    SET_LOGIC(&cell, someBool);

    // !!! Should some types not need an engine field?
    if (not engine)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}

Logic::operator bool() const {
    return VAL_INT32(&cell);
}



//
// CHARACTER
//

bool Value::isCharacter() const {
    return IS_CHAR(&cell);
}

Value::Value (char c, Engine * engine) noexcept :
    Value (Dont::Initialize)
{
    if (c < 0)
        throw std::runtime_error("Non-ASCII char passed to Value::Value()");

    SET_CHAR(&cell, static_cast<REBUNI>(c));

    // !!! Should some types not need an engine field?
    if (not engine)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}

Value::Value (wchar_t wc, Engine * engine) noexcept :
    Value (Dont::Initialize)
{
    SET_CHAR(&cell, wc);

    // !!! Should some types not need an engine field?
    if (not engine)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
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


unsigned long Character::codepoint() const {
    REBUNI uni = VAL_CHAR(&cell);
    // will probably not throw in Red, either
    return uni;
}


#if REN_CLASSLIB_QT
Character::operator QChar () const {
    REBUNI uni = VAL_CHAR(&cell);
    return QChar(uni);
}
#endif



//
// INTEGER
//

bool Value::isInteger() const {
    return IS_INTEGER(&cell);
}

Value::Value (int someInt, Engine * engine) noexcept :
    Value (Dont::Initialize)
{
    SET_INTEGER(&cell, someInt);

    // !!! Should some types not need an engine field?
    if (not engine)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}

Integer::operator int() const {
    return VAL_INT32(&cell);
}



//
// FLOAT
//

bool Value::isFloat() const {
    return IS_DECIMAL(&cell);
}

Value::Value (double someDouble, Engine * engine) noexcept :
    Value (Dont::Initialize)
{
    SET_DECIMAL(&cell, someDouble);

    // !!! Should some types not need an engine field?
    if (not engine)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}

Float::operator double() const {
    return VAL_DECIMAL(&cell);
}



//
// DATE
//

bool Value::isDate() const {
    return IS_DATE(&cell);
}


} // end namespace ren
