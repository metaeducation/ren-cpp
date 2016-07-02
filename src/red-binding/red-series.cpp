#include "rencpp/value.hpp"
#include "rencpp/atoms.hpp"
#include "rencpp/arrays.hpp"
#include "rencpp/function.hpp"
#include "rencpp/strings.hpp"

#include "rencpp/red.hpp"

#define UNUSED(x) static_cast<void>(x)

namespace ren {



bool AnySeries::isValid(RenCell const * cell) {
    UNUSED(cell);
    throw std::runtime_error("AnySeries::isValid not implemented");
}


///
/// ITERATION
///


void ren::internal::AnySeries_::operator++() {
    throw std::runtime_error("AnySeries_ iteration not implemented");

    /*cell.data.series.index++;*/
}


void ren::internal::AnySeries_::operator--() {
    throw std::runtime_error("AnySeries_ iteration not implemented");

    /* cell.data.series.index--; */
}


void ren::internal::AnySeries_::operator++(int) {
    ++*this;
}


void ren::internal::AnySeries_::operator--(int) {
    --*this;
}


AnyValue ren::internal::AnySeries_::operator*() const {
    throw std::runtime_error("AnySeries_::operator*() coming soon...");

/*
    AnyValue result {Dont::Initialize};

    if (hasType<String>(*this)) {
        // from str_to_char in Rebol source
        SET_CHAR(
            &result.cell,
            GET_ANY_CHAR(VAL_SERIES(&cell), cell.data.series.index)
        );
    } else if (hasType<AnyArray>(*this)) {
        result.cell = *VAL_BLK_SKIP(&cell, cell.data.series.index);
    } else {
        // Binary and such, would return an integer
        UNREACHABLE_CODE();
    }
    result.finishInit(origin);
    return result;*/
}


AnyValue ren::internal::AnySeries_::operator->() const {
    return *(*this);
}


void ren::internal::AnySeries_::head() {
/*    cell.data.series.index = static_cast<REBCNT>(0);*/
    throw std::runtime_error("AnySeries_::head coming soon...");
}


void ren::internal::AnySeries_::tail() {
/*    cell.data.series.index
        = static_cast<REBCNT>(cell.data.series.series->tail); */
    throw std::runtime_error("AnySeries_::tail coming soon...");
}


size_t AnySeries::length() const {
    throw std::runtime_error("AnySeries::length not implemented");

    /*REBINT index = (REBINT)VAL_INDEX(&cell);
    REBINT tail = (REBINT)VAL_TAIL(&cell);
    return tail > index ? tail - index : 0;*/
}


AnyValue AnySeries::operator[](AnyValue const & index) const {
    if (not hasType<Integer>(index))
        throw std::runtime_error("operator[] currently integers only...");

    // Terrible placeholder implementation (helps with testing in any case)
    // See Rebol implementation for another take

    size_t count = static_cast<size_t>(static_cast<Integer>(index));
    if (count > length())
        return none;

    auto it = begin();
    while (count > 1) {
        count--;
        it++;
    }

    return *it;
}


} // end namespace ren
