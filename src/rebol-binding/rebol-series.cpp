#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/indivisibles.hpp"
#include "rencpp/series.hpp"


namespace ren {

///
/// TYPE DETECTION
///


bool Value::isSeries() const {
    return isAnyBlock() or isAnyString() /* or isBinary()*/;
}



///
/// ITERATION
///


void ren::internal::Series_::operator++() {
    cell.data.series.index++;
}


void ren::internal::Series_::operator--() {
    cell.data.series.index--;
}


void ren::internal::Series_::operator++(int) {
    ++*this;
}


void ren::internal::Series_::operator--(int) {
    --*this;
}


Value ren::internal::Series_::operator*() const {
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
    return result;
}


Value ren::internal::Series_::operator->() const {
    return *(*this);
}


void ren::internal::Series_::head() {
    cell.data.series.index = static_cast<REBCNT>(0);
}


void ren::internal::Series_::tail() {
    cell.data.series.index
        = static_cast<REBCNT>(cell.data.series.series->tail);
}


size_t Series::length() const {
    REBINT index = (REBINT)VAL_INDEX(&cell);
    REBINT tail = (REBINT)VAL_TAIL(&cell);
    return tail > index ? tail - index : 0;
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
