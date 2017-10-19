#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/atoms.hpp"
#include "rencpp/engine.hpp"

#include "common.hpp"


namespace ren {

bool Atom::isValid(REBVAL const * cell) {
    // Will be more efficient when atom makes it formally into the
    // Rebol base typesets.
    //
    // !!! Review handling of void.  It is not considered an atom, correct?
    //
    return (
        IS_BLANK(cell)
        || IS_LOGIC(cell)
        || IS_CHAR(cell)
        || IS_INTEGER(cell)
        || IS_DECIMAL(cell)
        || IS_DATE(cell)
    );
}



//
// BLANK
//

bool Blank::isValid(REBVAL const * cell) {
    return IS_BLANK(cell);
}

AnyValue::AnyValue (blank_t, Engine * engine) noexcept :
    AnyValue (Dont::Initialize)
{
    Init_Blank(cell);

    // !!! Should some types not need an engine field?
    if (engine == nullptr)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}



//
// LOGIC
//

bool Logic::isValid(REBVAL const * cell) {
    return IS_LOGIC(cell);
}

bool AnyValue::isTruthy() const {
    return IS_TRUTHY(cell);
}

bool AnyValue::isFalsey() const {
    return IS_FALSEY(cell);
}

AnyValue::AnyValue (bool someBool, Engine * engine) noexcept :
    AnyValue (Dont::Initialize)
{
    Init_Logic(cell, someBool ? TRUE : FALSE);

    // !!! Should some types not need an engine field?
    if (engine == nullptr)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}

Logic::operator bool() const {
    return VAL_LOGIC(cell);
}



//
// CHARACTER
//

bool Character::isValid(REBVAL const * cell) {
    return IS_CHAR(cell);
}

AnyValue::AnyValue (char c, Engine * engine) :
    AnyValue (Dont::Initialize)
{
    if (c < 0)
        throw std::runtime_error("Non-ASCII char passed to AnyValue::AnyValue()");

    Init_Char(cell, static_cast<REBUNI>(c));

    // !!! Should some types not need an engine field?
    if (engine == nullptr)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}

AnyValue::AnyValue (wchar_t wc, Engine * engine) noexcept :
    AnyValue (Dont::Initialize)
{
    Init_Char(cell, wc);

    // !!! Should some types not need an engine field?
    if (engine == nullptr)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}

Character::operator char () const {
    REBUNI uni = VAL_CHAR(cell);
    if (uni > 127)
        throw std::runtime_error("Non-ASCII codepoint cast to char");
    return static_cast<char>(uni);
}


Character::operator wchar_t () const {
    REBUNI uni = VAL_CHAR(cell);
    // will throw in Red for "astral plane" unicode codepoints
    return static_cast<wchar_t>(uni);
}


unsigned long Character::codepoint() const {
    REBUNI uni = VAL_CHAR(cell);
    // will probably not throw in Red, either
    return uni;
}


#if REN_CLASSLIB_QT
Character::operator QChar () const {
    REBUNI uni = VAL_CHAR(cell);
    return QChar(uni);
}
#endif



//
// INTEGER
//

bool Integer::isValid(REBVAL const * cell) {
    return IS_INTEGER(cell);
}

AnyValue::AnyValue (int someInt, Engine * engine) noexcept :
    AnyValue (Dont::Initialize)
{
    Init_Integer(cell, someInt);

    // !!! Should some types not need an engine field?
    if (engine == nullptr)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}

Integer::operator int() const {
    // !!! How to correctly support 64-bit coercions?  Throw if out of range?
    int i = VAL_INT32(cell);
    return i;
}



//
// FLOAT
//

bool Float::isValid(REBVAL const * cell) {
    return IS_DECIMAL(cell);
}

AnyValue::AnyValue (double someDouble, Engine * engine) noexcept :
    AnyValue (Dont::Initialize)
{
    Init_Decimal(cell, someDouble);

    // !!! Should some types not need an engine field?
    if (engine == nullptr)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}

Float::operator double() const {
    return VAL_DECIMAL(cell);
}



//
// DATE
//

bool Date::isValid(REBVAL const * cell) {
    return IS_DATE(cell);
}


} // end namespace ren
