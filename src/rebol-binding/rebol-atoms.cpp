#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/atoms.hpp"
#include "rencpp/engine.hpp"

#include "rebol-common.hpp"


namespace ren {

bool Atom::isValid(RenCell const * cell) {
    // Will be more efficient when atom makes it formally into the
    // Rebol base typesets.
    //
    // !!! Review handling of void.  It is not considered an atom, correct?
    //
    return (
        IS_BLANK(AS_C_REBVAL(cell))
        || IS_LOGIC(AS_C_REBVAL(cell))
        || IS_CHAR(AS_C_REBVAL(cell))
        || IS_INTEGER(AS_C_REBVAL(cell))
        || IS_DECIMAL(AS_C_REBVAL(cell))
        || IS_DATE(AS_C_REBVAL(cell))
    );
}



//
// BLANK
//

bool Blank::isValid(RenCell const * cell) {
    return IS_BLANK(AS_C_REBVAL(cell));
}

AnyValue::AnyValue (blank_t, Engine * engine) noexcept :
    AnyValue (Dont::Initialize)
{
    SET_BLANK(AS_REBVAL(cell));

    // !!! Should some types not need an engine field?
    if (not engine)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}



//
// LOGIC
//

bool Logic::isValid(RenCell const * cell) {
    return IS_LOGIC(AS_C_REBVAL(cell));
}

bool AnyValue::isTrue() const {
    return IS_CONDITIONAL_TRUE(AS_C_REBVAL(cell));
}

bool AnyValue::isFalse() const {
    return IS_CONDITIONAL_FALSE(AS_C_REBVAL(cell));
}

AnyValue::AnyValue (bool someBool, Engine * engine) noexcept :
    AnyValue (Dont::Initialize)
{
    SET_LOGIC(AS_REBVAL(cell), someBool ? TRUE : FALSE);

    // !!! Should some types not need an engine field?
    if (not engine)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}

Logic::operator bool() const {
    return VAL_INT32(AS_C_REBVAL(cell));
}



//
// CHARACTER
//

bool Character::isValid(RenCell const * cell) {
    return IS_CHAR(AS_C_REBVAL(cell));
}

AnyValue::AnyValue (char c, Engine * engine) :
    AnyValue (Dont::Initialize)
{
    if (c < 0)
        throw std::runtime_error("Non-ASCII char passed to AnyValue::AnyValue()");

    SET_CHAR(AS_REBVAL(cell), static_cast<REBUNI>(c));

    // !!! Should some types not need an engine field?
    if (not engine)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}

AnyValue::AnyValue (wchar_t wc, Engine * engine) noexcept :
    AnyValue (Dont::Initialize)
{
    SET_CHAR(AS_REBVAL(cell), wc);

    // !!! Should some types not need an engine field?
    if (not engine)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}

Character::operator char () const {
    REBUNI uni = VAL_CHAR(AS_C_REBVAL(cell));
    if (uni > 127)
        throw std::runtime_error("Non-ASCII codepoint cast to char");
    return static_cast<char>(uni);
}


Character::operator wchar_t () const {
    REBUNI uni = VAL_CHAR(AS_C_REBVAL(cell));
    // will throw in Red for "astral plane" unicode codepoints
    return static_cast<wchar_t>(uni);
}


unsigned long Character::codepoint() const {
    REBUNI uni = VAL_CHAR(AS_C_REBVAL(cell));
    // will probably not throw in Red, either
    return uni;
}


#if REN_CLASSLIB_QT
Character::operator QChar () const {
    REBUNI uni = VAL_CHAR(AS_C_REBVAL(cell));
    return QChar(uni);
}
#endif



//
// INTEGER
//

bool Integer::isValid(RenCell const * cell) {
    return IS_INTEGER(AS_C_REBVAL(cell));
}

AnyValue::AnyValue (int someInt, Engine * engine) noexcept :
    AnyValue (Dont::Initialize)
{
    SET_INTEGER(AS_REBVAL(cell), someInt);

    // !!! Should some types not need an engine field?
    if (not engine)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}

Integer::operator int() const {
    // !!! How to correctly support 64-bit coercions?  Throw if out of range?
    int i = VAL_INT32(AS_C_REBVAL(cell));
    return i;
}



//
// FLOAT
//

bool Float::isValid(RenCell const * cell) {
    return IS_DECIMAL(AS_C_REBVAL(cell));
}

AnyValue::AnyValue (double someDouble, Engine * engine) noexcept :
    AnyValue (Dont::Initialize)
{
    SET_DECIMAL(AS_REBVAL(cell), someDouble);

    // !!! Should some types not need an engine field?
    if (not engine)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}

Float::operator double() const {
    return VAL_DECIMAL(AS_C_REBVAL(cell));
}



//
// DATE
//

bool Date::isValid(RenCell const * cell) {
    return IS_DATE(AS_C_REBVAL(cell));
}


} // end namespace ren
