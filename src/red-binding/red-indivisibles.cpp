#include "rencpp/value.hpp"
#include "rencpp/indivisibles.hpp"
#include "rencpp/blocks.hpp"
#include "rencpp/function.hpp"

#include "rencpp/red.hpp"

#define UNUSED(x) static_cast<void>(x)

namespace ren {


inline RedEngineHandle ensureEngine(Engine * engine) {
    return engine ? engine->getHandle() : Engine::runFinder().getHandle();
}


///
/// UNSET
///

bool Value::isUnset() const {
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_UNSET;
}


Value::Value (unset_t, Engine * engine) :
    Value (
        RedRuntime::makeCell4(RedRuntime::TYPE_UNSET, 0, 0, 0),
        ensureEngine(engine)
    )
{
}



///
/// NONE
///

bool Value::isNone() const {
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_NONE;
}


Value::Value (none_t, Engine * engine) :
    Value (
        RedRuntime::makeCell4(RedRuntime::TYPE_NONE, 0, 0, 0),
        ensureEngine(engine)
    )
{
}



///
/// LOGIC
///

bool Value::isLogic() const {
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_LOGIC;
}


bool Value::isTrue() const {
    return isLogic() and cell.data1;
}


bool Value::isFalse() const {
    return isLogic() and not cell.data1;
}


Value::Value (bool b, Engine * engine) :
    Value (
        RedRuntime::makeCell4(RedRuntime::TYPE_LOGIC, b, 0, 0),
        ensureEngine(engine)
    )
{
} // not 64-bit aligned, historical accident, TBD before 1.0


Logic::operator bool () const {
    return cell.s.data2;
}



///
/// CHARACTER
///

bool Value::isCharacter() const {
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

bool Value::isInteger() const {
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_INTEGER;
}


Value::Value (int i, Engine * engine) :
    Value (
        RedRuntime::makeCell4(RedRuntime::TYPE_INTEGER, 0, i, 0),
        ensureEngine(engine)
    )
{
} // 64-bit aligned for the someInt value


Integer::operator int () const {
    return cell.s.data2;
}



///
/// FLOAT
///

bool Value::isFloat() const {
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_FLOAT;
}


Value::Value (double d, Engine * engine) :
    Value (
        RedRuntime::makeCell3(RedRuntime::TYPE_FLOAT, 0, d),
        ensureEngine(engine)
    )
{
}


Float::operator double () const {
    return cell.dataD;
}


} // end namespace ren
