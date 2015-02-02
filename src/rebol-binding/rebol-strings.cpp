#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/strings.hpp"
#include "rencpp/engine.hpp"


namespace ren {

///
/// TYPE DETECTION AND INITIALIZATION
///


bool Value::isString(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_STRING);
        return true;
    }
    return IS_STRING(&cell);
}

bool Value::isTag(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_TAG);
        return true;
    }
    return IS_TAG(&cell);
}

bool Value::isFilename(REBVAL * init) const {
    if (init) {
        VAL_SET(init, REB_FILE);
        return true;
    }
    return IS_FILE(&cell);
}

bool Value::isAnyString() const {
    return IS_STRING(&cell) or IS_TAG(&cell)
        or IS_FILE(&cell) or IS_URL(&cell);
}



///
/// CONSTRUCTION
///

AnyString::AnyString (
    char const * spelling,
    internal::CellFunction cellfun,
    Engine * engine
) :
    Series (Dont::Initialize)
{
    (this->*cellfun)(&this->cell);

    if (not engine)
        engine = &Engine::runFinder();

    std::string source;

    if (isString()) {
        source += '{';
        source += spelling;
        source += '}';
    }
    else if (isTag()) {
        source += '<';
        source += spelling;
        source += '>';
    }
    else if (isFilename()) {
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
    : Series(Dont::Initialize)
{
    (this->*cellfun)(&this->cell);

    QString source;

    // Note: wouldn't be able to return char * without intermediate
    // http://stackoverflow.com/questions/17936160/

    if (isString()) {
        source += '{';
        source += spelling;
        source += '}';
    }
    else if (isTag()) {
        source += '<';
        source += spelling;
        source += '>';
    }
    else
        UNREACHABLE_CODE();

    QByteArray utf8 = source.toUtf8();
    internal::Loadable loadable (utf8.data());

    if (not engine)
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



///
/// EXTRACTION
///

std::string AnyString::spellingOf_STD() const {
    std::string result = static_cast<std::string>(*this);
    if (isString() /* or isUrl() or isEmail() or isFile() */)
        return result;
    if (isTag()) {
        result.erase(0, 1);
        return result.erase(result.length() - 1, 1);
    }
    throw std::runtime_error {"Invalid String Type"};
}


#if REN_CLASSLIB_QT

QString AnyString::spellingOf_QT() const {
    QString result = static_cast<QString>(*this);
    if (isString() /* or isUrl() or isEmail() or isFile() */)
        return result;
    if (isTag()) {
        assert(result.length() >= 2);
        return result.mid(1, result.length() - 2);
    }
    throw std::runtime_error {"Invalid String Type"};
}

#endif


} // end namespace ren
