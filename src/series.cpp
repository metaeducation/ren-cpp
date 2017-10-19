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


AnyValue AnySeries::operator[](AnyValue const & index)
const {
    // See notes on semantic questions about SELECT vs PICK for the meaning
    // of operator[] here, and why we go with "whatever path selection does"
    //
    //    https://github.com/hostilefork/rencpp/issues/64
    //
    // If there are complaints about this choice, it probably points to the
    // idea that Path selection needs to be addressed.  (Not that the
    // operator[] in the C++ binding needs to outsmart it.  :-P)

    // Note this code isn't as simple as:
    //
    //    return GetPath {*this, index, origin}.apply();
    //
    // Because there is a difference between an Engine object and a
    // RenEngineHandle...and as an internal all we know for this object
    // is its handle.  Finding the engine for that handle hasn't been
    // necessary anywhere else, and would involve some kind of tracking map.
    // So we do what building a path would do here.

    AnyValue getPath {Dont::Initialize};
    VAL_RESET_HEADER(getPath.cell, REB_GET_PATH);

    std::array<internal::Loadable, 2> loadables {{
        *this, index
    }};

    constructOrApplyInitialize(
        origin, // use our engine handle
        nullptr, // no context
        nullptr, // no applicand
        loadables.data(),
        loadables.size(),
        &getPath, // Do construct
        nullptr // Don't apply
    );

    ASSERT_VALUE_MANAGED(getPath.cell);

    AnyValue result {Dont::Initialize};

    // Need to wrap this in a try, and figure out a way to translate the
    // errors...

    constructOrApplyInitialize(
        origin, // use our engine handle
        nullptr, // no context
        &getPath, // path is the applicand
        nullptr,
        0,
        nullptr, // Don't construct
        &result // Do apply
    );

    return result;
}


} // end namespace ren
