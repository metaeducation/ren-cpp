#include <array>
#include <stdexcept>

#include "rencpp/value.hpp"
#include "rencpp/atoms.hpp"
#include "rencpp/series.hpp"
#include "rencpp/arrays.hpp" // For Path evaluation in operator[]

#include "rebol-common.hpp"


namespace ren {

//
// TYPE DETECTION
//


bool AnySeries::isValid(RenCell const & cell) {
    return ANY_SERIES(AS_C_REBVAL(&cell));
}



//
// ITERATION
//


void ren::internal::AnySeries_::operator++() {
    AS_REBVAL(&cell)->data.position.index++;
}


void ren::internal::AnySeries_::operator--() {
    AS_REBVAL(&cell)->data.position.index--;
}


void ren::internal::AnySeries_::operator++(int) {
    ++*this;
}


void ren::internal::AnySeries_::operator--(int) {
    --*this;
}


AnyValue ren::internal::AnySeries_::operator*() const {
    AnyValue result {Dont::Initialize};

    if (ANY_STR(AS_C_REBVAL(&cell))) {
        // from str_to_char in Rebol source
        SET_CHAR(
            AS_REBVAL(&result.cell),
            GET_ANY_CHAR(
                VAL_SERIES(AS_C_REBVAL(&cell)),
                AS_C_REBVAL(&cell)->data.position.index
            )
        );
    } else if (Is_Array_Series(VAL_SERIES(AS_C_REBVAL(&cell)))) {
        result.cell = *AS_C_RENCELL(VAL_BLK_SKIP(
            AS_C_REBVAL(&cell), AS_C_REBVAL(&cell)->data.position.index
        ));
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
    AS_REBVAL(&cell)->data.position.index = 0;
}


void ren::internal::AnySeries_::tail() {
    AS_REBVAL(&cell)->data.position.index
        = AS_REBVAL(&cell)->data.position.series->tail;
}


size_t AnySeries::length() const {
    REBCNT index = VAL_INDEX(AS_C_REBVAL(&cell));
    REBCNT tail = VAL_TAIL(AS_C_REBVAL(&cell));
    return tail > index ? tail - index : 0;
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
    VAL_SET(AS_REBVAL(&getPath.cell), REB_GET_PATH);

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

    ASSERT_VALUE_MANAGED(AS_REBVAL(&getPath.cell));

    AnyValue result {Dont::Initialize};

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
