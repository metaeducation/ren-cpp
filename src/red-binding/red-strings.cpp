#include "rencpp/value.hpp"
#include "rencpp/strings.hpp"

#include "rencpp/red.hpp"

#define UNUSED(x) static_cast<void>(x)

namespace ren {

///
/// TYPE CHECKING AND INITIALIZATION
///

bool Value::isString(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_STRING;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_STRING;
}


bool Value::isTag(RenCell *) const {
    throw std::runtime_error("tag not implemented");
}


bool Value::isFilename(RenCell *) const {
    throw std::runtime_error("file not implemented");
}


bool Value::isAnyString() const {
    switch (RedRuntime::getDatatypeID(this->cell)) {
        case RedRuntime::TYPE_STRING:
        case RedRuntime::TYPE_FILE:
        case RedRuntime::TYPE_URL:
            return true;
        default:
            break;
    }
    return false;
}



///
/// CONSTRUCTION
///

AnyString::AnyString(
    char const * cstr,
    internal::CellFunction cellfun,
    Engine * engine
) :
    Series (Dont::Initialize)
{
    throw std::runtime_error("AnyString::AnyString coming soon...");

    UNUSED(cstr);
    UNUSED(cellfun);
    UNUSED(engine);
}



///
/// EXTRACTION
///

#if REN_CLASSLIB_STD == 1

AnyString::operator std::string() const {
    return to_string(*this);
}

#endif



} // end namespace ren
