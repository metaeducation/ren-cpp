#include "rencpp/value.hpp"
#include "rencpp/atoms.hpp"
#include "rencpp/arrays.hpp"
#include "rencpp/function.hpp"

#include "rencpp/red.hpp"


namespace ren {


///
/// TYPE CHECKING AND INITIALIZATION
///


bool Error::isValid(RenCell const &) {
    throw std::runtime_error("errors not implemented");
}


} // end namespace ren
