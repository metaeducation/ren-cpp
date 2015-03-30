#include <array>
#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/indivisibles.hpp"
#include "rencpp/series.hpp"
#include "rencpp/blocks.hpp" // For Path evaluation in operator[]


namespace ren {

//
// TYPE DETECTION
//


bool Value::isSeries() const {
    return isAnyBlock() or isAnyString() /* or isBinary()*/;
}



//
// ITERATION
//


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


Value Series::operator[](Value const & index)
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

    Value getPath {Dont::Initialize};
    Value::isGetPath(&getPath.cell);

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

    Value result {Dont::Initialize};

    // Need to wrap this in a try, and figure out a way to translate the
    // errors based on whether you have REN_RUNTIME or not

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
