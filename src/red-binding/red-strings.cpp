#include "rencpp/value.hpp"
#include "rencpp/strings.hpp"

#include "rencpp/red.hpp"

#define UNUSED(x) static_cast<void>(x)

namespace ren {

//
// TYPE CHECKING
//

bool String::isValid(RedCell const & cell) {
    return RedRuntime::getDatatypeID(cell) == RedRuntime::TYPE_STRING;
}


bool Tag::isValid(RedCell const & cell) {
    UNUSED(cell);
    throw std::runtime_error("tag not implemented");
}


bool Filename::isValid(RedCell const & cell) {
    UNUSED(cell);
    throw std::runtime_error("file not implemented");
}


bool AnyString::isValid(RedCell const & cell) {
    switch (RedRuntime::getDatatypeID(cell)) {
        case RedRuntime::TYPE_STRING:
        case RedRuntime::TYPE_FILE:
        case RedRuntime::TYPE_URL:
            return true;
        default:
            break;
    }
    return false;
}


//
// TYPE FORMAT INITIALIZATION
//

void AnyString::initString(RedCell & cell) {
    cell.header = RedRuntime::TYPE_STRING;
}


void AnyString::initTag(RedCell & cell) {
    UNUSED(cell);
    throw std::runtime_error("tag not implemented");
}


void AnyString::initFilename(RedCell & cell) {
    UNUSED(cell);
    throw std::runtime_error("file not implemented");
}


///
/// CONSTRUCTION
///

AnyString::AnyString(
    char const * cstr,
    internal::CellFunction cellfun,
    Engine * engine
) :
    AnySeries (Dont::Initialize)
{
    throw std::runtime_error("AnyString::AnyString coming soon...");

    UNUSED(cstr);
    UNUSED(cellfun);
    UNUSED(engine);
}


#if REN_CLASSLIB_QT == 1

AnyString::AnyString (
    QString const & spelling,
    internal::CellFunction cellfun,
    Engine * engine
)
    : AnySeries(Dont::Initialize)
{
    throw std::runtime_error("AnyString::AnyString coming soon...");

    UNUSED(spelling);
    UNUSED(cellfun);
    UNUSED(engine);
}

#endif


} // end namespace ren
