#include <iostream>
#include <sstream>

#include <csignal>
#include <unistd.h>

#include "rencpp/rebol.hpp"

// Implemented in rebol-runtime.cpp - we don't really do anything other than
// keep the Rebol request states of open/closed happy, it's a stub and we
// feed to the C++ iostreams which take care of opening themselves

extern "C" void Open_StdIO();


namespace ren {


RebolRuntime runtime {true};
Runtime & runtimeRef = runtime;

namespace internal {

Loadable::Loadable (char const * sourceCstr) :
    Value (Value::Dont::Initialize)
{
    // using REB_END as our "alien"
    VAL_SET(&cell, REB_END);
    cell.data.integer = evilPointerToInt32Cast(sourceCstr);

    refcountPtr = nullptr;
    origin = REN_ENGINE_HANDLE_INVALID;
}

} // end namespace internal


bool Runtime::needsRefcount(REBVAL const & cell) {
    return ANY_SERIES(&cell);
}



///
/// HOST BARE BONES HOOKS
///

// This is redundant with the host lib crash hook...except it takes one
// parameter.  It seems to be called only from those bare-bones I/O
// functions in order to avoid a dependency on the host lib itself

extern "C" void Host_Crash(REBYTE * message);
void Host_Crash(REBYTE * message) {
    throw std::runtime_error(
        std::string(reinterpret_cast<char *>(message))
    );
}



///
/// REBOL INITIALIZATION
///

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


RebolRuntime::RebolRuntime (bool someExtraInitFlag) :
    Runtime (),
    initialized (false),
    osPtr (&std::cout),
    isPtr (&std::cin)
{
    UNUSED(someExtraInitFlag);

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
                std::string(reinterpret_cast<char const *>(title))
                + " : " + reinterpret_cast<char const *>(content)
            );
        };
}



REBVAL RebolRuntime::loadAndBindWord(
    REBSER * context,
    const char * cstrUtf8,
    REBOL_Types kind,
    size_t len
) {
    REBVAL result;

    // Set_Word sets the fields of an ANY_WORD cell, but doesn't set
    // the header bits.

    Set_Word(
        &result,
        // Make_Word has a misleading name; it just finds the symbol
        // number (a REBINT) and the rest of this is needed in order
        // to get to a well formed word.
        Make_Word(
            reinterpret_cast<REBYTE*>(const_cast<char*>(cstrUtf8)),
            len == 0 ? strlen(cstrUtf8) : len
        ),
        // Initialize FRAME to null
        nullptr,
        0
    );

    // Set the "cell type" and clear the other header flags
    // (SET_TYPE would leave the other header flags alone, but they
    // are still uninitialized data at this point)

    VAL_SET(&result, kind);

    // The word is now well formed, but unbound.  If you supplied a
    // context we will bind it here.

    if (context)
        Bind_Word(context, &result);

    // Note that with C++11 move semantics, this is constructed in place;
    // in other words, the caller's REBVAL that they process in the return
    // was always the memory "result" used.  Quick evangelism note is that
    // this means objects can *contain* pointers and manage the lifetime
    // while not costing more to pass around.  If you want value semantics
    // you just use them.

    return result;
}

bool RebolRuntime::lazyInitializeIfNecessary() {
    if (initialized)
        return false;

    //
    // I thought this stack measuring thing could be done in the constructor,
    // but Rebol isn't prepared to have more than one thread, maybe?  It
    // seems maybe something is setup when you call init and if you try to
    // call Make_Series from another thread it complains.  Look into it.
    //

    int marker;
    REBCNT bounds = OS_CONFIG(1, 0);

    if (bounds == 0)
        bounds = STACK_BOUNDS;

#ifdef OS_STACK_GROWS_UP
    Stack_Limit = (REBCNT)(&marker) + bounds;
#else
    if (bounds > (REBCNT)(&marker))
        Stack_Limit = 100;
    else
        Stack_Limit = (REBCNT)(&marker) - bounds;
#endif

    // Rebytes, version numbers
    // REBOL_VER
    // REBOL_REV
    // REBOL_UPD
    // REBOL_SYS
    // REBOL_VAR


    // Although you can build and pass a REBARGS structure yourself and
    // set appropriate defaults, the easiest idea is to formulate your
    // request in terms of the argc and argv an executable would have
    // gotten and use the Parse_Args API.
    //
    // Right now we take all defaults, but put in an informative "we
    // don't have a way to give you the executable's path if we are
    // a binding"

    REBCHR fakeArgv0 []
        = "/dev/null/rencpp-binding/look-at/rebol-hooks.cpp";

    REBCHR argvQuiet []
        = "--quiet";

    // Theoretically we could offer hooks for this deferred initialization
    // to pass something here, or even change it while running.  Right
    // now we offer an informative fake path that is (hopefully) harmless.
    //
    // Of course, we all know what the long term security impacts are of
    // putting "informative garbage" in slots is...

    REBCHR * argv[] {fakeArgv0, argvQuiet};

    // There are (R)ebol (O)ption (F)lags telling it various things,
    // in Main_Args.options, but I seem to have better luck with
    // just letting it parse the ordinary command line args.

    REBARGS Main_Args;
    Parse_Args(2, argv, &Main_Args);

    Init_Core(&Main_Args);

    GC_Active = TRUE; // Turn on GC
    if (Main_Args.options & RO_TRACE) {
        Trace_Level = 9999;
        Trace_Flags = 1;
    }

    // In the old way of thinking, this must be done before a console I/O can
    // occur.  However now that we are hooking through to C++ iostreams with
    // a replacement rebol-stdio.cpp (instead of os/posix/dev-stdio.cpp) that
    // isn't really necessary.

    Open_StdIO();

    // Set up the interrupt handler for things like Ctrl-C (which had
    // previously been stuck in with stdio, because that's where the signals
    // were coming from...like Ctrl-C at the keyboard).  Rencpp is more
    // general, and if you are performing an evaluation in a GUI and want
    // to handle a Ctrl-C on the gui thread during an infinite loop you need
    // to be doing the evaluation on a worker thread and signal it from GUI

    auto signalHandler = [](int sig) {
        UNUSED(sig);
        std::cout << "[escape]";
        SET_SIGNAL(SIG_ESCAPE);
    };

    signal(SIGINT, signalHandler);
#ifdef SIGHUP
    signal(SIGHUP, signalHandler);
#endif
    signal(SIGTERM, signalHandler);

    // Initialize the REBOL library (reb-lib):
    if (!CHECK_STRUCT_ALIGN)
        throw std::runtime_error(
            "RebolHooks: Incompatible struct alignment"
        );


    // bin is optional startup code (compressed).  If it is provided, it
    // will be stored in system/options/boot-host, loaded, and evaluated.

    REBYTE * startupBin = nullptr;
    REBINT len = 0; // length of above bin

    if (startupBin) {
        REBSER spec;
        spec.data = startupBin;
        spec.tail = len;
        spec.rest = 0;
        spec.info = 0;
        spec.size = 0;

        REBSER * startup = Decompress(&spec, 0, -1, 10000000, 0);

        if (not startup)
            throw std::runtime_error("RebolHooks: Bad startup code");;

        Set_Binary(BLK_SKIP(Sys_Context, SYS_CTX_BOOT_HOST), startup);
    }

    if (Init_Mezz(0) != 0 /* "zero for success" */)
        throw std::runtime_error("RebolHooks: Mezzanine startup failure");

    // There is an unfortunate property of QUIT which is that it calls
    // Halt_Code, which uses a jump buffer Halt_State which is static to
    // c-do.c - hence we cannot catch QUIT.  Unless...we rewrite the value
    // it is set to, to use a "fake quit" that shares the jump buffer we
    // use for execution...

    REBVAL quitWord = loadAndBindWord(Lib_Context, "quit");

    REBVAL quitNative = *Get_Var(&quitWord);

    if (not IS_NATIVE(&quitNative)) {

        // If you look at sys-value.h and check out the definition of
        // REBHDR, you will see that the order of fields depends on the
        // endianness of the platform.  So if the C build and the C++
        // build do not have the same #define for ENDIAN_LITTLE then they
        // will wind up disagreeing.  You might have successfully loaded
        // a function but have the bits scrambled.  So before telling you
        // we can't find "load" in the Mezzanine, we look for the
        // scrambling in question and tell you that's what the problem is

        if (quitNative.flags.flags.resv == REB_NATIVE) {
            throw std::runtime_error(
                "Bit field order swap detected..."
                " Did you compile Rebol with a different setting for"
                " ENDIAN_LITTLE?  Check reb-config.h"
            );
        }

        // If that wasn't the problem, it was something else.

        throw std::runtime_error(
            "Couldn't get QUIT native for unknown reason"
        );
    }

    // Tweak the bits to put our fake quit function in and save it back...
    // This worked for QUIT but unfortunately couldn't work for Escape/CtrlC
    // So a one-word modification to Rebol is necessary

/*    quitNative.data.func.func.code = &internal::Fake_Quit;
    Set_Var(&quitWord, &quitNative); */

    initialized = true;
    return true;
}



void RebolRuntime::doMagicOnlyRebolCanDo() {
   std::cout << "REBOL MAGIC!\n";
}


void RebolRuntime::cancel() {
    SET_SIGNAL(SIG_ESCAPE);
}


std::ostream & RebolRuntime::setOutputStream(std::ostream & os) {
    auto temp = osPtr;
    osPtr = &os;
    return *temp;
}


std::istream & RebolRuntime::setInputStream(std::istream & is) {
    auto temp = isPtr;
    isPtr = &is;
    return *temp;
}


std::ostream & RebolRuntime::getOutputStream() {
    return *osPtr;
}


std::istream & RebolRuntime::getInputStream() {
    return *isPtr;
}


RebolRuntime::~RebolRuntime () {
    OS_QUIT_DEVICES(0);
}

} // end namespace ren


