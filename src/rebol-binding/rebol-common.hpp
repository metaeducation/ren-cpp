#ifndef RENCPP_REBOL_COMMON_HPP
#define RENCPP_REBOL_COMMON_HPP

#ifdef _WIN32
    //
    // On Windows it is required to include <windows.h>, and defining the
    // _WIN32_WINNT constant to 0x0501 specifies the minimum targeted version
    // is Windows XP.  This is the earliest platform API still supported by
    // Visual Studio 2015:
    //
    //     https://msdn.microsoft.com/en-us/library/6sehtctf.aspx
    //
    // R3-Alpha used 0x0500, indicating a minimum target of Windows 2000.  No
    // Windows-XP-specific dependencies were added in Ren-C, but the version
    // was bumped to avoid compilation errors in the common case.
    //
    // !!! Note that %sys-core.h includes <windows.h> as well if building
    // for windows.  The redundant inclusion should not create a problem.
    // (So better to do the inclusion just to test that it doesn't.)
    //
    #undef _WIN32_WINNT
    #define _WIN32_WINNT 0x0501
    #include <windows.h>

    // Put any dependencies that include <windows.h> here
    //
    /* #include "..." */
    /* #include "..." */

    // Undefine the Windows version of IS_ERROR to avoid compiler warning
    // when Rebol redefines it.  (Rebol defines IS_XXX for all datatypes.)
    //
    #undef IS_ERROR
    #undef max
    #undef min
#else
    #include <signal.h> // needed for SIGINT, SIGTERM, SIGHUP
#endif

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
    REBARR *args,
    REBSPC *specifier
);

#endif // RENCPP_REBOL_COMMON_HPP
