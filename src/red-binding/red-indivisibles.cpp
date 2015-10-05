#include "rencpp/value.hpp"
#include "rencpp/atoms.hpp"
#include "rencpp/arrays.hpp"
#include "rencpp/function.hpp"

#include "rencpp/red.hpp"

#define UNUSED(x) static_cast<void>(x)

namespace ren {


inline RedEngineHandle ensureEngine(Engine * engine) {
    return engine ? engine->getHandle() : Engine::runFinder().getHandle();
}



///
/// NONE
///

bool AnyValue::isNone() const {
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_NONE;
}


AnyValue::AnyValue (none_t, Engine * engine) noexcept  :
    AnyValue (
        RedRuntime::makeCell4I(RedRuntime::TYPE_NONE, 0, 0, 0),
        ensureEngine(engine)
    )
{
}



///
/// LOGIC
///

bool AnyValue::isLogic() const {
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_LOGIC;
}


bool AnyValue::isTrue() const {
    return isLogic() and cell.data1;
}


bool AnyValue::isFalse() const {
    return isLogic() and not cell.data1;
}


AnyValue::AnyValue (bool b, Engine * engine) noexcept :
    AnyValue (
        RedRuntime::makeCell4I(RedRuntime::TYPE_LOGIC, b, 0, 0),
        ensureEngine(engine)
    )
{
} // not 64-bit aligned, historical accident, TBD before 1.0


Logic::operator bool () const {
    return cell.dataII.data2;
}



///
/// CHARACTER
///

bool AnyValue::isCharacter() const {
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_CHAR;
}


Character::operator char() const {
    throw std::runtime_error("Character::operator char() coming soon...");
}


Character::operator wchar_t() const {
    throw std::runtime_error("Character::operator wchar_t() coming soon...");
}



///
/// INTEGER
///

bool AnyValue::isInteger() const {
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_INTEGER;
}


AnyValue::AnyValue (int i, Engine * engine) noexcept :
    AnyValue (
        RedRuntime::makeCell4I(RedRuntime::TYPE_INTEGER, 0, i, 0),
        ensureEngine(engine)
    )
{
} // 64-bit aligned for the someInt value


Integer::operator int () const {
    return cell.dataII.data2;
}



///
/// FLOAT
///

bool AnyValue::isFloat() const {
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_FLOAT;
}


AnyValue::AnyValue (double d, Engine * engine) noexcept :
    AnyValue (
        RedRuntime::makeCell2I1D(RedRuntime::TYPE_FLOAT, 0, d),
        ensureEngine(engine)
    )
{
}


Float::operator double () const {
    return cell.dataD;
}


} // end namespace ren
