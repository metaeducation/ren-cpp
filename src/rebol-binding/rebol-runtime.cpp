#include <iostream>
#include <stdexcept>

#include <csignal>

#include "rencpp/engine.hpp"
#include "rencpp/rebol.hpp"
#include "rencpp/arrays.hpp"

#include "rebol-common.hpp"


extern "C" {
#include "rebol/src/include/reb-ext.h"
#include "rebol/src/include/reb-lib.h"

#ifdef TO_WINDOWS
    #include <windows.h>

    // The object files from Rebol linked into Ren/C++ need a variable named
    // App_Instance when built for Windows.  This is used by the host code
    // to create an invisible HWND which handles events via its message
    // queue.  We thus have to initialize it if we want things like devices
    // and http/https to work, so we use GetModuleHandle(NULL).
    //
    extern HINSTANCE App_Instance;
    HINSTANCE App_Instance = 0;
#else
    #include <unistd.h>
#endif

// Saphirion additions with commands for running https
extern void Init_Core_Ext();
extern void Shutdown_Core_Ext(void);

// See comments in rebol-os-lib-table.cpp
extern REBOL_HOST_LIB Host_Lib_Init;
}


#ifndef MAX_PATH
#define MAX_PATH 4096  // from host-lib.c, generally lacking in Posix
#endif


// The binding was built on an idea that did not exist in R3-Alpha, which is
// that of being able to take the APPLY idea for functions and apply to
// other types.  So for instance, `apply (quote x:) [1 + 2]` would wind
// up having a behavior for a SET-WORD! to assign the quanity to X.  Each
// type was defined to have an APPLY behavior, with the exclusion of ERROR!
//
REBOOL Generalized_Apply_Throws(
    REBVAL *out,
    const REBVAL *applicand,
    REBARR *args,
    REBSPC *specifier
) {
    if (applicand && IS_ERROR(applicand)) {
        if (ARR_LEN(args) != 0) {
            // If you try to "apply" an error with an arguments block, that
            // will give you an error about the apply (not the error itself)

            fail (Error_Invalid_Arg_Core(ARR_HEAD(args), specifier));
        }

        fail (VAL_CONTEXT(applicand));
    }

    // If it's an object (context) then "apply" means run the code
    // but bind it inside the object.  Suggestion by @WiseGenius:
    //
    // http://chat.stackoverflow.com/transcript/message/20530463#20530463

    if (applicand && IS_OBJECT(applicand)) {
        REBARR * reboundArgs = Copy_Array_Deep_Managed(args, specifier);
        PUSH_GUARD_ARRAY(reboundArgs);

        // Note this takes a C array of values terminated by a REB_END.
        Bind_Values_Set_Midstream_Shallow(
            ARR_HEAD(reboundArgs),
            VAL_CONTEXT(applicand)
        );

        if (Do_At_Throws(out, reboundArgs, 0, SPECIFIED)) { // was copied
            DROP_GUARD_ARRAY(reboundArgs);
            return TRUE;
        }

        DROP_GUARD_ARRAY(reboundArgs);

        return FALSE;
    }

    // Just put the value at the head of a DO chain.
    //

    REBIXO indexor = Do_Array_At_Core(
        out,
        applicand,
        args,
        0,
        specifier,
        DO_FLAG_NORMAL
    );

    if (indexor == THROWN_FLAG)
        return TRUE;

    if (indexor != END_FLAG) {
        //
        // If the DO chain didn't end, then consider that an error.  So
        // generalized apply of FOO: with `1 + 2 3` will do the addition,
        // satisfy FOO, but have a 3 left over unused.  If FOO: were a
        // function being APPLY'd, you'd say that was too many arguments
        //
        fail (Error(RE_APPLY_TOO_MANY));
    }

    return FALSE;
}


namespace ren {


RebolRuntime runtime {true};

static REBVAL loadAndBindWord(
    REBCTX * context,
    unsigned char const * nameUtf8,
    size_t lenBytes,
    enum Reb_Kind kind
) {
    REBVAL word;
    Init_Any_Word(&word, kind, Intern_UTF8_Managed(nameUtf8, lenBytes));

    // The word is now well formed, but unbound.  If you supplied a
    // context we will bind it here.

    if (context) {
        REBCNT index = Try_Bind_Word(context, &word);
        assert(index != 0);
    }

    // Note that with C++11 move semantics, this is constructed in place;
    // in other words, the caller's REBVAL that they process in the return
    // was always the memory "result" used.  Quick evangelism note is that
    // this means objects can *contain* pointers and manage the lifetime
    // while not costing more to pass around.  If you want value semantics
    // you just use them.

    return word;
}


//
// REBOL INITIALIZATION
//

//
// The code in the initialization is an unwinding of the RL_init code from
// a-lib.c (which we do not include in the binding)
//
// We don't call Init_Core until a lazy opportunity on the first creation
// of an environment.  This is because we want client applications to have
// a simple interface by default, yet be able to pass in parameters to an
// initializer if needed.  (For instance, passing in the working directory,
// if they want the bound program to know about that kind of thing.)
//


RebolRuntime::RebolRuntime (bool) :
    Runtime (),
    initialized (false)
{
    Host_Lib = &Host_Lib_Init; // OS host library (dispatch table)

    // We don't want to rewrite the entire host lib here, but we can
    // hook functions in.  It's good to have a debug hook point here
    // for when things crash, rather than sending to the default
    // host lib implementation.

    Host_Lib->os_crash =
        [](REBYTE const * title, REBYTE const * content) {
            // This is our only error hook for certain types of "crashes"
            // (evaluation_error is caught elsewhere).  If we want to
            // break crashes down more specifically (without touching Rebol
            // source) we'd have to parse the error strings to translate them
            // into exception classes.  Left as an exercise for the reader

            throw std::runtime_error(
                std::string(cs_cast(title)) + " : " + cs_cast(content)
            );
        };

    // Make sure our opaque types stay in sync with their "real" variants

    assert(sizeof(Reb_Value) == sizeof(RenCell));

    assert(sizeof(uint32_t) == sizeof(REBCNT));
    assert(sizeof(int32_t) == sizeof(REBINT));
    assert(sizeof(intptr_t) == sizeof(REBIPT));
    assert(sizeof(uintptr_t) == sizeof(REBUPT));
}


bool RebolRuntime::lazyInitializeIfNecessary() {

    if (initialized)
        return false;

#ifdef OS_STACK_GROWS_UP
    Stack_Limit = static_cast<void*>(-1);
#else
    Stack_Limit = 0;
#endif

#ifdef TO_WINDOWS
    App_Instance = GetModuleHandle(NULL);
#endif

    Init_Core();

    initialized = true;

    // Set up the interrupt handler for things like Ctrl-C (which had
    // previously been stuck in with stdio, because that's where the signals
    // were coming from...like Ctrl-C at the keyboard).  Rencpp is more
    // general, and if you are performing an evaluation in a GUI and want
    // to handle a Ctrl-C on the gui thread during an infinite loop you need
    // to be doing the evaluation on a worker thread and signal it from GUI

    auto signalHandler = [](int) {
        Engine::runFinder().getOutputStream() << "[escape]";
        SET_SIGNAL(SIG_HALT); // SIG_BREAK?
    };

    signal(SIGINT, signalHandler);
#ifdef SIGHUP
    signal(SIGHUP, signalHandler);
#endif
    signal(SIGTERM, signalHandler);

    // This is a small demo of a low-level extension that does not use Ren-C++
    // methods could be written, if it were necessary.  (It used to be needed
    // for "Generalized Apply", but then wasn't needed...so a residual stub
    // is kept here to keep the technique in order if it becomes necessary
    // for some other patch.)

    REBYTE test_name[] = "test-rencpp-low-level-hook";

    REBVAL testWord = loadAndBindWord(
        Lib_Context, test_name, LEN_BYTES(test_name), REB_WORD
    );

    const REBYTE testSpecStr[] = {
        "{Ren-C++ demo of a test extension function.}"
        " value {Some value}"
        " /refine {Some refinement}"
        " arg {Some refinement arg}"
    };

    REBNAT testDispatcher = [](Reb_Frame* frame_) -> REB_R {
        PARAM(1, value);
        REFINE(2, refine);
        PARAM(3, arg);

        REBVAL * out = D_OUT;

        // Used to be APPLY replacement, but now just a placeholder if
        // something like that winds up being needed.
        SET_BLANK(out);

        return R_OUT;
    };

    REBVAL testSpec;
    Init_Block(
        &testSpec,
        Scan_UTF8_Managed(testSpecStr, LEN_BYTES(testSpecStr))
    );

    REBFUN *testNative = Make_Function(
        Make_Paramlist_Managed_May_Fail(&testSpec, MKF_KEYWORDS),
        testDispatcher,
        NULL // no underlying function, this is foundational
    );
    *Sink_Var_May_Fail(&testWord, SPECIFIED) = *FUNC_VALUE(testNative);

    return true;
}



void RebolRuntime::doMagicOnlyRebolCanDo() {
   std::cout << "REBOL MAGIC!\n";
}


void RebolRuntime::cancel() {
    SET_SIGNAL(SIG_HALT); // SIG_BREAK and debugging...?
}


RebolRuntime::~RebolRuntime () {
    if (initialized) {
        OS_QUIT_DEVICES(0);

        Shutdown_Core();
    }
}

} // end namespace ren


