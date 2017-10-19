#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/strings.hpp"
#include "rencpp/engine.hpp"

#include "common.hpp"


namespace ren {

//
// TYPE DETECTION
//


bool String::isValid(REBVAL const * cell) {
    return IS_STRING(cell);
}

bool Tag::isValid(REBVAL const * cell) {
    return IS_TAG(cell);
}

bool Filename::isValid(REBVAL const * cell) {
    return IS_FILE(cell);
}

bool AnyString::isValid(REBVAL const * cell) {
    return ANY_STRING(cell);
}


//
// TYPE HEADER INITIALIZATION
//

void AnyString::initString(REBVAL *cell) {
    VAL_RESET_HEADER(cell, REB_STRING);
}

void AnyString::initTag(REBVAL *cell) {
    VAL_RESET_HEADER(cell, REB_TAG);
}

void AnyString::initFilename(REBVAL *cell) {
    VAL_RESET_HEADER(cell, REB_FILE);
}



//
// CONSTRUCTION
//

AnyString::AnyString (
    char const * spelling,
    internal::CellFunction cellfun,
    Engine * engine
) :
    AnySeries (Dont::Initialize)
{
    (*cellfun)(cell);

    if (engine == nullptr)
        engine = &Engine::runFinder();

    std::string source;

    if (hasType<String>(*this)) {
        source += '{';
        source += spelling;
        source += '}';
    }
    else if (hasType<Tag>(*this)) {
        source += '<';
        source += spelling;
        source += '>';
    }
    else if (hasType<Filename>(*this)) {
        source += "%";
        source += spelling;
    }
    else
        UNREACHABLE_CODE();

    internal::Loadable loadable (source.data());

    constructOrApplyInitialize(
        engine->getHandle(),
        nullptr, // no context
        nullptr, // no applicand
        &loadable,
        1,
        this, // Do construct
        nullptr // Don't apply
    );
}


AnyString::AnyString (
    std::string const & spelling,
    internal::CellFunction cellfun,
    Engine * engine
) :
    AnyString (spelling.c_str(), cellfun, engine)
{
}




#if REN_CLASSLIB_QT == 1

AnyString::AnyString (
    QString const & spelling,
    internal::CellFunction cellfun,
    Engine * engine
)
    : AnySeries(Dont::Initialize)
{
    (*cellfun)(cell);

    QString source;

    // Note: wouldn't be able to return char * without intermediate
    // http://stackoverflow.com/questions/17936160/

    if (hasType<String>(*this)) {
        source += '{';
        source += spelling;
        source += '}';
    }
    else if (hasType<Tag>(*this)) {
        source += '<';
        source += spelling;
        source += '>';
    }
    else
        UNREACHABLE_CODE();

    QByteArray utf8 = source.toUtf8();
    internal::Loadable loadable (utf8.data());

    if (engine == nullptr)
        engine = &Engine::runFinder();

    constructOrApplyInitialize(
        engine->getHandle(),
        nullptr, // no context
        nullptr, // no applicand
        &loadable,
        1,
        this, // do construct
        nullptr // don't apply
    );
}

#endif



//
// EXTRACTION
//

std::string AnyString::spellingOf_STD() const {
    std::string result = static_cast<std::string>(*this);
    if (hasType<String>(*this) /* or isUrl() or isEmail() or isFile() */)
        return result;
    if (hasType<Tag>(*this)) {
        result.erase(0, 1);
        return result.erase(result.length() - 1, 1);
    }
    throw std::runtime_error {"Invalid String Type"};
}


#if REN_CLASSLIB_QT

QString AnyString::spellingOf_QT() const {
    QString result = static_cast<QString>(*this);
    if (hasType<String>(*this) /* or isUrl() or isEmail() or isFile() */)
        return result;
    if (hasType<Tag>(*this)) {
        assert(result.length() >= 2);
        return result.mid(1, result.length() - 2);
    }
    throw std::runtime_error {"Invalid String Type"};
}

#endif


} // end namespace ren
