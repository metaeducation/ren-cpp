#include "rencpp/values.hpp"
#include "rencpp/indivisibles.hpp"
#include "rencpp/blocks.hpp"
#include "rencpp/function.hpp"

#include "rencpp/red.hpp"

#define UNUSED(x) static_cast<void>(x)

namespace ren {



bool Value::isSeries() const {
    return isAnyBlock() or isAnyString();
}


///
/// ITERATION
///


void ren::internal::Series_::operator++() {
    throw std::runtime_error("Series_ iteration not implemented");

    /*cell.data.series.index++;*/
}


void ren::internal::Series_::operator--() {
    throw std::runtime_error("Series_ iteration not implemented");

    /* cell.data.series.index--; */
}


void ren::internal::Series_::operator++(int) {
    ++*this;
}


void ren::internal::Series_::operator--(int) {
    --*this;
}


Value ren::internal::Series_::operator*() const {
    throw std::runtime_error("Series_::operator*() coming soon...");

/*
    Value result {Dont::Initialize};

    if (isAnyString()) {
        // from str_to_char in Rebol source
        SET_CHAR(
            &result.cell,
            GET_ANY_CHAR(VAL_SERIES(&cell), cell.data.series.index)
        );
    } else if (isAnyBlock()) {
        result.cell = *VAL_BLK_SKIP(&cell, cell.data.series.index);
    } else {
        // Binary and such, would return an integer
        UNREACHABLE_CODE();
    }
    result.finishInit(origin);
    return result;*/
}


Value ren::internal::Series_::operator->() const {
    return *(*this);
}


void ren::internal::Series_::head() {
/*    cell.data.series.index = static_cast<REBCNT>(0);*/
    throw std::runtime_error("Series_::head coming soon...");
}


void ren::internal::Series_::tail() {
/*    cell.data.series.index
        = static_cast<REBCNT>(cell.data.series.series->tail); */
    throw std::runtime_error("Series_::tail coming soon...");
}


size_t Series::length() const {
    throw std::runtime_error("series::length not implemented");

    /*REBINT index = (REBINT)VAL_INDEX(&cell);
    REBINT tail = (REBINT)VAL_TAIL(&cell);
    return tail > index ? tail - index : 0;*/
}


Value Series::operator[](size_t index) const {
    // Terrible placeholder implementation (helps with testing in any case)

    if (index > length())
        return none;

    size_t count = index;
    auto it = begin();
    while (count > 1) {
        count--;
        it++;
    }

    return *it;
}


} // end namespace ren
