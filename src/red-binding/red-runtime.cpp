#include <iostream>
#include <sstream>

#ifndef NDEBUG
#include <map>
#include <stdexcept>
#endif

#include "rencpp/red.hpp"

#define UNUSED(x) static_cast<void>(x)

namespace ren {

RedRuntime runtime (true);

Runtime & runtimeRef = runtime;


internal::Loadable::Loadable (char const * sourceCstr) :
    AnyValue (AnyValue::Dont::Initialize)
{
    cell = RedRuntime::makeCell2I1P(
        RedRuntime::TYPE_ALIEN,
        0,
        const_cast<char *>(sourceCstr)
    );
    next = prev = nullptr;
    origin = REN_ENGINE_HANDLE_INVALID;
}


RedRuntime::DatatypeID RedRuntime::getDatatypeID(RedCell const & cell) {
    // extract the lowest byte
    return static_cast<RedRuntime::DatatypeID>(cell.header & 0xFF);
}


RedRuntime::RedRuntime (bool someExtraInitFlag) : Runtime () {
    UNUSED(someExtraInitFlag);
}


void RedRuntime::doMagicOnlyRedCanDo() {
   std::cout << "RED MAGIC!\n";
}


RedRuntime::~RedRuntime () {
}


#ifndef NDEBUG
std::string RedRuntime::datatypeName(ren::RedRuntime::DatatypeID id) {
    // Implementing this as a map in case the values are to change...
    // of course if the values change, then the DatatypeID values
    // will need to be updated.

    static std::map<RedRuntime::DatatypeID, char const *> names = {
        {RedRuntime::DatatypeID::TYPE_ALIEN, "TYPE_(VALUE/ALIEN)"},
        {RedRuntime::DatatypeID::TYPE_DATATYPE, "TYPE_DATATYPE"},
        {RedRuntime::DatatypeID::TYPE_UNSET, "TYPE_UNSET"},
        {RedRuntime::DatatypeID::TYPE_NONE, "TYPE_NONE"},
        {RedRuntime::DatatypeID::TYPE_LOGIC, "TYPE_LOGIC"},
        {RedRuntime::DatatypeID::TYPE_BLOCK, "TYPE_BLOCK"},
        {RedRuntime::DatatypeID::TYPE_STRING, "TYPE_STRING"},
        {RedRuntime::DatatypeID::TYPE_INTEGER, "TYPE_INTEGER"},
        {RedRuntime::DatatypeID::TYPE_SYMBOL, "TYPE_SYMBOL"},
        {RedRuntime::DatatypeID::TYPE_CONTEXT, "TYPE_CONTEXT"},
        {RedRuntime::DatatypeID::TYPE_WORD, "TYPE_WORD"},
        {RedRuntime::DatatypeID::TYPE_SET_WORD, "TYPE_SET_WORD"},
        {RedRuntime::DatatypeID::TYPE_LIT_WORD, "TYPE_LIT_WORD"},
        {RedRuntime::DatatypeID::TYPE_GET_WORD, "TYPE_GET_WORD"},
        {RedRuntime::DatatypeID::TYPE_REFINEMENT, "TYPE_REFINEMENT"},
        {RedRuntime::DatatypeID::TYPE_CHAR, "TYPE_CHAR"},
        {RedRuntime::DatatypeID::TYPE_NATIVE, "TYPE_NATIVE"},
        {RedRuntime::DatatypeID::TYPE_ACTION, "TYPE_ACTION"},
        {RedRuntime::DatatypeID::TYPE_OP, "TYPE_OP"},
        {RedRuntime::DatatypeID::TYPE_FUNCTION, "TYPE_FUNCTION"},
        {RedRuntime::DatatypeID::TYPE_PATH, "TYPE_PATH"},
        {RedRuntime::DatatypeID::TYPE_LIT_PATH, "TYPE_LIT_PATH"},
        {RedRuntime::DatatypeID::TYPE_SET_PATH, "TYPE_SET_PATH"},
        {RedRuntime::DatatypeID::TYPE_GET_PATH, "TYPE_GET_PATH"},
        {RedRuntime::DatatypeID::TYPE_PAREN, "TYPE_PAREN"},
        {RedRuntime::DatatypeID::TYPE_ROUTINE, "TYPE_ROUTINE"},
        {RedRuntime::DatatypeID::TYPE_ISSUE, "TYPE_ISSUE"},
        {RedRuntime::DatatypeID::TYPE_FILE, "TYPE_FILE"},
        {RedRuntime::DatatypeID::TYPE_URL, "TYPE_URL"},
        {RedRuntime::DatatypeID::TYPE_BITSET, "TYPE_BITSET"},
        {RedRuntime::DatatypeID::TYPE_POINT, "TYPE_POINT"},
        {RedRuntime::DatatypeID::TYPE_OBJECT, "TYPE_OBJECT"},
        {RedRuntime::DatatypeID::TYPE_FLOAT, "TYPE_FLOAT"},
        {RedRuntime::DatatypeID::TYPE_BINARY, "TYPE_BINARY"},

        {RedRuntime::DatatypeID::TYPE_TYPESET, "TYPE_TYPESET"},
        {RedRuntime::DatatypeID::TYPE_ERROR, "TYPE_ERROR"},

        {RedRuntime::DatatypeID::TYPE_CLOSURE, "TYPE_CLOSURE"},

        {RedRuntime::DatatypeID::TYPE_PORT, "TYPE_PORT"}
    };

    auto it = names.find(id);
    if (it == end(names)) {
        std::stringstream ss;
        ss << "No match for DatatypeID " << static_cast<int>(id)
            << "in operator<< for Runtime::DatatypeID in debug.cpp";
        throw std::runtime_error(ss.str());
    }

    return (*it).second;
}

#endif

} // end namespace ren
