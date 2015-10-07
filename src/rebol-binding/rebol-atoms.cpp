#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/atoms.hpp"
#include "rencpp/engine.hpp"


namespace ren {

bool Atom::isValid(RenCell const & cell) {
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
// NONE
//

bool None::isValid(RenCell const & cell) {
    return IS_NONE(&cell);
}

AnyValue::AnyValue (none_t, Engine * engine) noexcept :
    AnyValue (Dont::Initialize)
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

bool Logic::isValid(RenCell const & cell) {
    return IS_LOGIC(&cell);
}

bool AnyValue::isTrue() const {
    return IS_CONDITIONAL_TRUE(&cell);
}

bool AnyValue::isFalse() const {
    return IS_CONDITIONAL_FALSE(&cell);
}

AnyValue::AnyValue (bool someBool, Engine * engine) noexcept :
    AnyValue (Dont::Initialize)
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

bool Character::isValid(RenCell const & cell) {
    return IS_CHAR(&cell);
}

AnyValue::AnyValue (char c, Engine * engine) noexcept :
    AnyValue (Dont::Initialize)
{
    if (c < 0)
        throw std::runtime_error("Non-ASCII char passed to AnyValue::AnyValue()");

    SET_CHAR(&cell, static_cast<REBUNI>(c));

    // !!! Should some types not need an engine field?
    if (not engine)
        engine = &Engine::runFinder();

    finishInit(engine->getHandle());
}

AnyValue::AnyValue (wchar_t wc, Engine * engine) noexcept :
    AnyValue (Dont::Initialize)
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

bool Integer::isValid(RenCell const & cell) {
    return IS_INTEGER(&cell);
}

AnyValue::AnyValue (int someInt, Engine * engine) noexcept :
    AnyValue (Dont::Initialize)
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

bool Float::isValid(RenCell const & cell) {
    return IS_DECIMAL(&cell);
}

AnyValue::AnyValue (double someDouble, Engine * engine) noexcept :
    AnyValue (Dont::Initialize)
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

bool Date::isValid(RenCell const & cell) {
    return IS_DATE(&cell);
}


} // end namespace ren
