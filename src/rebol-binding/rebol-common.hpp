#ifndef RENCPP_REBOL_COMMON_HPP
#define RENCPP_REBOL_COMMON_HPP

#include "rebol/src/include/sys-core.h"

inline REBVAL const * AS_C_REBVAL(RenCell const * cell) {
    return reinterpret_cast<REBVAL const *>(cell);
}
inline REBVAL * AS_REBVAL(RenCell * cell) {
    return reinterpret_cast<REBVAL *>(cell);
}

inline RenCell const * AS_C_RENCELL(REBVAL const * rebval) {
    return reinterpret_cast<RenCell const *>(rebval);
}
inline RenCell * AS_RENCELL(REBVAL * rebval) {
    return reinterpret_cast<RenCell *>(rebval);
}

// !!! This functionality will likely be added to make APPLY work in a more
// general fashion (and not just on functions).

extern REBOOL Generalized_Apply_Throws(
    REBVAL *out,
    const REBVAL *applicand,
    REBARR *args
);

#endif // RENCPP_REBOL_COMMON_HPP
