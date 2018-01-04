#include <array>
#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/atoms.hpp"
#include "rencpp/series.hpp"
#include "rencpp/arrays.hpp" // For Path evaluation in operator[]

#include "common.hpp"


namespace ren {

//
// TYPE DETECTION
//


bool AnySeries::isValid(REBVAL const * cell) {
    return ANY_SERIES(cell);
}



//
// ITERATION
//


void ren::internal::AnySeries_::operator++() {
    cell->payload.any_series.index++;
}


void ren::internal::AnySeries_::operator--() {
    cell->payload.any_series.index--;
}


void ren::internal::AnySeries_::operator++(int) {
    ++*this;
}


void ren::internal::AnySeries_::operator--(int) {
    --*this;
}


AnyValue ren::internal::AnySeries_::operator*() const {
    AnyValue result {Dont::Initialize};

    if (0 == VAL_LEN_AT(cell)) {
        Init_Void(result.cell);
    }
    else if (ANY_STRING(cell)) {
        // from str_to_char in Rebol source
        Init_Char(
            cell,
            GET_ANY_CHAR(VAL_SERIES(cell), VAL_INDEX(cell))
        );
    } else if (GET_SER_FLAG(VAL_SERIES(cell), SERIES_FLAG_ARRAY)) {
        Derelativize(
            result.cell,
            ARR_AT(VAL_ARRAY(cell), VAL_INDEX(cell)), VAL_SPECIFIER(cell)
        );
    } else {
        // Binary and such, would return an integer
        UNREACHABLE_CODE();
    }
    result.finishInit(origin);
    return result;
}


AnyValue ren::internal::AnySeries_::operator->() const {
    return *(*this);
}


void ren::internal::AnySeries_::head() {
    cell->payload.any_series.index = 0;
}


void ren::internal::AnySeries_::tail() {
    cell->payload.any_series.index = VAL_LEN_HEAD(cell);
}


size_t AnySeries::length() const {
    return VAL_LEN_AT(cell);
}


AnyValue AnySeries::operator[](AnyValue const & picker) const {
    //
    // See notes on semantic questions of operator[] here, and why we go with
    // "whatever GET-PATH! selection does", e.g. PICK.
    //
    //    https://github.com/hostilefork/rencpp/issues/64
    //
    REBVAL *picked = rebDo(
        BLANK_VALUE, // superflous blank not needed in UTF-8 Everywhere branch
        rebEval(NAT_VALUE(pick_p)),
        cell,
        picker.cell,
        END
    );

    AnyValue result = AnyValue::fromCell_<AnyValue>(picked, origin);
    rebFree(picked);
    return result;
}


} // end namespace ren
