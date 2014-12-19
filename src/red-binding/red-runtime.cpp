#include <iostream>
#include <sstream>

#ifndef NDEBUG
#include <map>
#include <stdexcept>
#include <sstream>
#endif

#include "redcpp/red.hpp"


namespace ren {

RedRuntime runtime (true);

Runtime & runtimeRef = runtime;


internal::Loadable::Loadable (char const * sourceCstr) :
    Value (Value::Dont::Initialize)
{
    cell = RedRuntime::makeCell4(
        RedRuntime::TYPE_ALIEN,
        evilPointerToInt32Cast(sourceCstr),
        0, 0
    );
    refcountPtr = nullptr;
    engine = REN_ENGINE_HANDLE_INVALID;
}


RedRuntime::DatatypeID RedRuntime::getDatatypeID(RedCell const & cell) {
    // extract the lowest byte
    return static_cast<RedRuntime::DatatypeID>(cell.header & 0xFF);
}


bool Runtime::needsRefcount(RenCell const & cell) {
    //
    // ANY-WORD! is currently not ever GC'd.  Ideally they would be, as this
    // means the symbol table will just grow indefinitely and hold words you're
    // not still using.
    //
    // Changing this entry in the table is what would be required to indicate
    // a held-onto Word.  Although a lighter approach might be taken to just
    // link all the words into a chain as symbol refcounting may be more rare,
    // and have a callback hook from the runtime to ask for the list?
    //
    bool needsRefcountTable[] = {
        false, // TYPE_VALUE a.k.a TYPE_ALIEN
        false, // TYPE_DATATYPE
        false, // TYPE_UNSET
        false, // TYPE_NONE
        false, // TYPE_LOGIC
        true, // TYPE_BLOCK
        true, // TYPE_STRING
        false, // TYPE_INTEGER
        false, // TYPE_SYMBOL (???)
        true, // TYPE_CONTEXT (???)
        false, // TYPE_WORD
        false, // TYPE_SET_WORD
        false, // TYPE_LIT_WORD
        false, // TYPE_GET_WORD
        false, // TYPE_REFINEMENT
        false, // TYPE_CHAR
        false, // TYPE_NATIVE
        false, // TYPE_ACTION
        false, // TYPE_OP
        true, // TYPE_FUNCTION
        true, // TYPE_PATH
        true, // TYPE_LIT_PATH
        true, // TYPE_SET_PATH
        true, // TYPE_GET_PATH
        true, // TYPE_PAREN
        true, // TYPE_ROUTINE
        true, // TYPE_ISSUE
        true, // TYPE_FILE
        true, // TYPE_URL
        true, // TYPE_BITSET
        false, // TYPE_POINT
        true, // TYPE_OBJECT
        false, // TYPE_FLOAT
        true, // TYPE_BINARY

        true, // TYPE_TYPESET
        true, // TYPE_ERROR

        true, // TYPE_CLOSURE

        true, // TYPE_PORT
    };

    return needsRefcountTable[RedRuntime::getDatatypeID(cell)];
}


std::string Runtime::form(Value const & value) {

    // Forming should be Engine-independent under these assumptions

    std::stringstream ss;
#ifndef NDEBUG
    ss << "Formed(" << RedRuntime::getDatatypeID(value) << ")";
#else
    ss << "Formed(" << static_cast<int>(RedRuntime::getDatatypeID(value)) << ")";
#endif
    return ss.str();
}


RedRuntime::RedRuntime (bool someExtraInitFlag) : Runtime () {
    UNUSED(someExtraInitFlag);
}


void RedRuntime::doMagicOnlyRedCanDo() {
   std::cout << "RED MAGIC!\n";
}


RedRuntime::~RedRuntime () {
}

} // end namespace ren



using ren::Value;

using ren::RedRuntime;

#ifndef NDEBUG
std::ostream & operator<<(std::ostream & os, ren::RedRuntime::DatatypeID id) {
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
    if (it != end(names)) {
        os << (*it).second;
    } else {
        std::stringstream ss;
        ss << "No match for DatatypeID " << static_cast<int>(id)
            << "in operator<< for Runtime::DatatypeID in debug.cpp";
        throw std::runtime_error(ss.str());
    }

    return os;
}

#endif
