#include "rencpp/value.hpp"
#include "rencpp/indivisibles.hpp"
#include "rencpp/blocks.hpp"
#include "rencpp/function.hpp"

#include "rencpp/red.hpp"


namespace ren {


///
/// TYPE CHECKING AND INITIALIZATION
///


bool Value::isError() const {
    throw std::runtime_error("errors not implemented");
}


} // end namespace ren
