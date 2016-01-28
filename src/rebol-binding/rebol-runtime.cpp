#include <iostream>
#include <stdexcept>

#include <csignal>
#include <unistd.h>

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
    HINSTANCE App_Instance = 0;
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
    REBARR *args
) {
    if (applicand && IS_ERROR(applicand)) {
        if (ARR_LEN(args) != 0) {
            // If you try to "apply" an error with an arguments block, that
            // will give you an error about the apply (not the error itself)

            fail (Error_Invalid_Arg(ARR_HEAD(args)));
        }

        fail (VAL_CONTEXT(applicand));
    }

    // If it's an object (context) then "apply" means run the code
    // but bind it inside the object.  Suggestion by @WiseGenius:
    //
    // http://chat.stackoverflow.com/transcript/message/20530463#20530463

    if (applicand && IS_OBJECT(applicand)) {
        REBARR * reboundArgs = Copy_Array_Deep_Managed(args);
        PUSH_GUARD_ARRAY(reboundArgs);

        // Note this takes a C array of values terminated by a REB_END.
        Bind_Values_Set_Forward_Shallow(
            ARR_HEAD(reboundArgs),
            VAL_CONTEXT(applicand)
        );

        if (Do_At_Throws(out, reboundArgs, 0)) {
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
        DO_FLAG_NEXT | DO_FLAG_LOOKAHEAD | DO_FLAG_EVAL_NORMAL
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

REBARGS rebargs;

static REBVAL loadAndBindWord(
    REBCTX * context,
    unsigned char const * nameUtf8,
    size_t lenBytes,
    enum Reb_Kind kind
) {
    REBVAL word;
    VAL_INIT_WRITABLE_DEBUG(&word);

    // !!! Make_Word has a misleading name; it allocates a symbol number

    Val_Init_Word(&word, kind, Make_Word(nameUtf8, lenBytes));

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

    // = {0} on construct should zero fill, but gives warnings in older gcc
    // http://stackoverflow.com/questions/13238635/#comment37115390_13238635

    rebargs.options = 0;
    rebargs.script = nullptr;
    rebargs.args = nullptr;
    rebargs.do_arg = nullptr;
    rebargs.version = nullptr;
    rebargs.debug = nullptr;
    rebargs.import = nullptr;
    rebargs.secure = nullptr;
    rebargs.boot = nullptr;
    rebargs.exe_path = nullptr;
    rebargs.home_dir = nullptr;

    // Rebytes, version numbers
    // REBOL_VER
    // REBOL_REV
    // REBOL_UPD
    // REBOL_SYS
    // REBOL_VAR

    // Make sure our opaque types stay in sync with their "real" variants

    assert(sizeof(Reb_Value) == sizeof(RenCell));

    assert(sizeof(uint32_t) == sizeof(REBCNT));
    assert(sizeof(int32_t) == sizeof(REBINT));
    assert(sizeof(intptr_t) == sizeof(REBIPT));
    assert(sizeof(uintptr_t) == sizeof(REBUPT));

    assert(offsetof(Reb_Call, cell) == offsetof(RenCall, cell));
    assert(offsetof(Reb_Call, func) == offsetof(RenCall, func));
    assert(offsetof(Reb_Call, dsp_orig) == offsetof(RenCall, dsp_orig));
    assert(offsetof(Reb_Call, flags) == offsetof(RenCall, flags));
    assert(offsetof(Reb_Call, out) == offsetof(RenCall, out));
    assert(offsetof(Reb_Call, value) == offsetof(RenCall, value));
    assert(offsetof(Reb_Call, eval_fetched) == offsetof(RenCall, eval_fetched));
    assert(offsetof(Reb_Call, source) == offsetof(RenCall, source));
    assert(offsetof(Reb_Call, indexor) == offsetof(RenCall, indexor));
    assert(offsetof(Reb_Call, label_sym) == offsetof(RenCall, label_sym));
    assert(offsetof(Reb_Call, frame) == offsetof(RenCall, frame));
    assert(offsetof(Reb_Call, param) == offsetof(RenCall, param));
    assert(offsetof(Reb_Call, arg) == offsetof(RenCall, arg));
    assert(offsetof(Reb_Call, refine) == offsetof(RenCall, refine));
    assert(offsetof(Reb_Call, prior) == offsetof(RenCall, prior));
    assert(offsetof(Reb_Call, mode) == offsetof(RenCall, mode));
    assert(offsetof(Reb_Call, expr_index) == offsetof(RenCall, expr_index));
    assert(sizeof(Reb_Call) == sizeof(RenCall));

    // function.hpp does a bit more of its template work in the include file
    // than we might like (but that's how templates work).  We don't want
    // to expose R_OUT and R_OUT_IS_THROWN (they are Rebol internals) so we
    // use REN_ constants as surrogates in that header.

    assert(R_OUT == REN_SUCCESS);
    assert(R_OUT_IS_THROWN == REN_APPLY_THREW);
}


bool RebolRuntime::lazyInitializeIfNecessary() {
    REBYTE vers[8];

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

    // Parse_Args has a memory leak; it uses OS_Get_Current_Dir which
    // mallocs a string which is never freed.  We hold onto the REBARGS
    // we use and control the initialization so we don't leak strings

    // There are (R)ebol (O)ption flags telling it various things,
    // in rebargs.options, we just don't want it to display the banner

    rebargs.options = RO_QUIET;

    // Theoretically we could offer hooks for this deferred initialization
    // to pass something here, or even change it while running.  Right
    // now for exe_path we offer an informative fake path that is (hopefully)
    // harmless.

    REBCHR const * exe_path_const = OS_STR_LIT(
        "/dev/null/rencpp-binding/look-at/rebol-hooks.cpp"
    );

    rebargs.exe_path = new REBCHR[OS_STRLEN(exe_path_const) + 1];
    OS_STRNCPY(rebargs.exe_path, exe_path_const, OS_STRLEN(exe_path_const) + 1);

    rebargs.home_dir = new REBCHR[MAX_PATH];

#ifdef TO_WINDOWS
    GetCurrentDirectory(MAX_PATH, reinterpret_cast<wchar_t *>(rebargs.home_dir));
#else
    getcwd(reinterpret_cast<char *>(rebargs.home_dir), MAX_PATH);
#endif

    Init_Core(&rebargs);

    // adds to a table used by RL_Start, must be called before
    //
    Init_Core_Ext();

    // Needed to run the SYS_START function
    int err_num = RL_START(0, 0, NULL, 0, 0);

    GC_Active = TRUE; // Turn on GC

    if (rebargs.options & RO_TRACE) {
        Trace_Level = 9999;
        Trace_Flags = 1;
    }


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

    // Initialize the REBOL library (reb-lib):
    if (not CHECK_STRUCT_ALIGN)
        throw std::runtime_error(
            "RebolHooks: Incompatible struct alignment..."
            " Did you build mainline Rebol on 64-bit instead of with -m32?"
            " (for 64-bit builds, use http://github.com/rebolsource/r3)"
        );


    // bin is optional startup code (compressed).  If it is provided, it
    // will be stored in system/options/boot-host, loaded, and evaluated.

    REBYTE * startupBin = nullptr;
    REBCNT len = 0; // length of above bin

    if (startupBin) {
        REBSER * startup = Decompress(startupBin, len, -1, FALSE, FALSE);

        if (not startup)
            throw std::runtime_error("RebolHooks: Bad startup code");;

        Val_Init_Binary(CTX_VAR(Sys_Context, SYS_CTX_BOOT_HOST), startup);
    }

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

    REBNAT testFun = [](Reb_Call* call_) -> REBCNT {
        PARAM(1, value);
        REFINE(2, refine);
        PARAM(3, arg);

        REBVAL * out = D_OUT;

        // Used to be APPLY replacement, but now just a placeholder if
        // something like that winds up being needed.
        SET_NONE(out);

        return R_OUT;
    };

    REBARR * testSpec = Scan_Source(testSpecStr, LEN_BYTES(testSpecStr));

    REBVAL testNative;
    VAL_INIT_WRITABLE_DEBUG(&testNative);

    Make_Native(&testNative, testSpec, testFun, REB_NATIVE, FALSE);
    *GET_MUTABLE_VAR_MAY_FAIL(&testWord) = testNative;

    initialized = true;

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

        Shutdown_Core_Ext();

        Shutdown_Core();

        // needs to last during Rebol run
        delete [] rebargs.exe_path;
        delete [] rebargs.home_dir;
    }
}

} // end namespace ren


