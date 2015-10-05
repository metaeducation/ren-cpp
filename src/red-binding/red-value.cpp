#include "rencpp/value.hpp"
#include "rencpp/atoms.hpp"
#include "rencpp/arrays.hpp"
#include "rencpp/function.hpp"

#include "rencpp/red.hpp"

using std::to_string;

#define UNUSED(x) static_cast<void>(x)

namespace ren {

///
/// COMPARISONS
///

bool AnyValue::isEqualTo(AnyValue const & other) const {
    UNUSED(other);

    throw std::runtime_error("AnyValue::isEqualTo coming soon...");
}


bool AnyValue::isSameAs(AnyValue const & other) const {
    UNUSED(other);

    throw std::runtime_error("AnyValue::isSameAs coming soon...");
}


///
/// INITIALIZATION FINISHER
///

bool AnyValue::tryFinishInit(RenEngineHandle engine) {
    // This needs to add tracking needed to protect from GC, see Rebol binding.
    origin = engine;
    return RedRuntime::getDatatypeID(this->cell) != RedRuntime::TYPE_UNSET;
}


void AnyValue::uninitialize() {
    // This needs to remove the protections from GC, see Rebol binding.
    // !!! Ultimately Rebol and Red are probably similar enough in needs that
    // the tracking might be moved to the common code.
}



///
/// STRING CONVERSIONS
///

std::string to_string(AnyValue const & value) {

    // placeholder implementation...

    if (value.isNone()) {
        return "#[none!]";
    }
    else if (value.isLogic()) {
        if (value.isTrue())
            return "#[true!]";
        return "#[false!]";
    }
    else if (value.isInteger()) {
        return std::to_string(value.cell.dataII.data2);
    }
    else if (value.isFloat()) {
        return std::to_string(value.cell.dataD);
    }
    else
        throw std::runtime_error("to_string unimplemented for datatype");
}

} // end namespace ren
