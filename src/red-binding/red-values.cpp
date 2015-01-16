#include "rencpp/values.hpp"
#include "rencpp/indivisibles.hpp"
#include "rencpp/blocks.hpp"
#include "rencpp/function.hpp"

#include "rencpp/red.hpp"

using std::to_string;

#define UNUSED(x) static_cast<void>(x)

namespace ren {

///
/// COMPARISONS
///

bool Value::isEqualTo(Value const & other) const {
    UNUSED(other);

    throw std::runtime_error("Value::isEqualTo coming soon...");
}


bool Value::isSameAs(Value const & other) const {
    UNUSED(other);

    throw std::runtime_error("Value::isSameAs coming soon...");
}


///
/// INITIALIZATION FINISHER
///

void Value::finishInit(RenEngineHandle engine) {
    if (needsRefcount()) {
        refcountPtr = new RefcountType (1);
    } else {
        refcountPtr = nullptr;
    }

    origin = engine;
}



///
/// STRING CONVERSIONS
///

#if REN_CLASSLIB_STD == 1

std::string to_string(Value const & value) {

    // placeholder implementation...

    if (value.isUnset()) {
        return "#[unset!]";
    }
    else if (value.isNone()) {
        return "#[none!]";
    }
    else if (value.isLogic()) {
        if (value.isTrue())
            return "#[true!]";
        return "#[false!]";
    }
    else if (value.isInteger()) {
        return std::to_string(value.cell.s.data2);
    }
    else if (value.isFloat()) {
        return std::to_string(value.cell.dataD);
    }
    else
        throw std::runtime_error("to_string unimplemented for datatype");
}

#endif

} // end namespace ren
