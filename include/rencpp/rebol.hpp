#ifndef RENCPP_REBOL_HPP
#define RENCPP_REBOL_HPP

#include "ren.hpp"

#ifndef NDEBUG
#include <unordered_map>
#endif

namespace ren {

// Not only is Runtime implemented on a per-binding basis
// (hence not requiring virtual methods) but you can add more
// specialized methods that are peculiar to just this runtime

class RebolRuntime : public Runtime {
private:
    Context * defaultContext;
    bool initialized;

    REBVAL rebvalLoadFunction; // can it be garbage collected?

private:
    static REBVAL loadAndBindWord(
        REBSER * context, // may be null, Lib_Context, SysContext, or Get_System(...CTX_USER...)
        const char * cstrUtf8, // strlen() is # of utf8 bytes, not C chars
        REBOL_Types kind = REB_WORD, // or REB_SET_WORD, REB_GET_WORD...
        size_t len = 0 // default to taking the byte length with strlen
    );

public:
    friend class internal::Loadable;

public:
    RebolRuntime (bool someExtraInitFlag);

    void doMagicOnlyRebolCanDo();

    void lazyInitializeIfNecessary();

    ~RebolRuntime() override;
};

extern RebolRuntime runtime;

#ifndef NDEBUG
namespace internal {
    extern std::unordered_map<
        decltype(RebolEngineHandle::data),
        std::unordered_map<REBSER const *, unsigned int>
    > nodes;
}
#endif

} // end namespace ren


namespace rebol = ren;

// See comments in rebol-os-lib-table.cpp
extern REBOL_HOST_LIB Host_Lib_Init;

#endif
