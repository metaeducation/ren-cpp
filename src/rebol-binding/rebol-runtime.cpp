#include <iostream>
#include <sstream>

#include "rencpp/rebol.hpp"

namespace ren {


RebolRuntime runtime {true};
Runtime & runtimeRef = runtime;



internal::Loadable::Loadable (char const * sourceCstr) :
    Value (Value::Dont::Initialize)
{
    // using REB_END as our "alien"
    VAL_SET(&cell, REB_END);
    cell.data.integer = evilPointerToInt32Cast(sourceCstr);

    refcountPtr = nullptr;
    origin = REN_ENGINE_HANDLE_INVALID;
}


bool Runtime::needsRefcount(REBVAL const & cell) {
    return ANY_SERIES(&cell);
}



///
/// HOST BARE BONES HOOKS
///


// Host bare-bones stdio functs:
extern "C" void Open_StdIO(void);
extern "C" void Put_Str(char *buf);
extern "C" REBYTE *Get_Str();

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
    initialized (false)
{
    UNUSED(someExtraInitFlag);

    Host_Lib = &Host_Lib_Init; // OS host library (dispatch table)

    // We don't want to rewrite the entire host lib here, but we can
    // hook functions in.  It's good to have a debug hook point here
    // for when things crash, rather than sending to the default
    // host lib implementation.

    Host_Lib->os_crash =
        [](REBYTE const * title, REBYTE const * content) {
            // This is our only error hook, and if we want to break errors
            // down more specifically (without touching Rebol source) we'd
            // have to parse the error strings to translate them into
            // exception classes.  Left as an exercise for the reader
            //
            // We do throw it as an evaluation_error, so you at least know
            // it was something wrong with the code you were evaluating and
            // not something else

            throw evaluation_error(
                std::string(reinterpret_cast<char const *>(title))
                + " : " + reinterpret_cast<char const *>(content)
            );
        };

    int marker;
    REBCNT bounds = static_cast<REBCNT>(OS_CONFIG(1, 0));

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
        static_cast<REBINT>(Make_Word(
            reinterpret_cast<REBYTE*>(const_cast<char*>(cstrUtf8)),
            len == 0 ? strlen(cstrUtf8) : len)
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

void RebolRuntime::lazyInitializeIfNecessary() {
    if (initialized)
        return;

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

    // Must be done before an console I/O can occur. Does not use reb-lib,
    // so this device should open even if there are other problems.

    Open_StdIO();  // also sets up interrupt handler

    // Initialize the REBOL library (reb-lib):
    if (!CHECK_STRUCT_ALIGN)
        throw std::runtime_error(
            "RebolHooks: Incompatible struct alignment"
        );


    // bin is optional startup code (compressed).  If it is provided, it
    // will be stored in system/options/boot-host, loaded, and evaluated.

    REBYTE *bin = nullptr;
    REBINT len = 0; // length of above bin

    if (bin) {
        REBSER spec;
        spec.data = bin;
        // conversion to 'REBCNT {aka long unsigned int}' from REBINT
        // '{aka long int}' may change the sign of the result
        spec.tail = static_cast<REBCNT>(len);
        spec.rest = 0;
        spec.info = 0;
        spec.size = 0;

        REBSER *ser = Decompress(&spec, 0, -1, 10000000, 0);

        if (!ser)
            throw std::runtime_error("RebolHooks: Bad startup code");;

        REBVAL *val = BLK_SKIP(Sys_Context, SYS_CTX_BOOT_HOST);
        Set_Binary(val, ser);
    }

    if (Init_Mezz(0) != 0 /* "zero for success" */)
        throw std::runtime_error("RebolHooks: Mezzanine startup failure");

    // Now that the mezzanine is loaded, we rely somewhat on LOAD for
    // functionality and it's not in the box.  So we need to save the
    // REBVAL for it, and maybe some others.

    REBVAL loadWord = loadAndBindWord(Lib_Context, "load");
    rebvalLoadFunction = *Get_Var(&loadWord);

    if (!IS_FUNCTION(&rebvalLoadFunction)) {

        // If you look at sys-value.h and check out the definition of
        // REBHDR, you will see that the order of fields depends on the
        // endianness of the platform.  So if the C build and the C++
        // build do not have the same #define for ENDIAN_LITTLE then they
        // will wind up disagreeing.  You might have successfully loaded
        // a function but have the bits scrambled.  So before telling you
        // we can't find "load" in the Mezzanine, we look for the
        // scrambling in question and tell you that's what the problem is

        if (rebvalLoadFunction.flags.flags.resv == REB_FUNCTION) {
            throw std::runtime_error(
                "Bit field order swap detected..."
                " Did you compile Rebol with a different setting for"
                " ENDIAN_LITTLE?  Check reb-config.h"
            );
        }

        // If that wasn't the problem, it was something else.

        throw std::runtime_error(
            "Couldn't get LOAD function from Mezzanine for unknown reason"
        );
    }

    initialized = true;
}



void RebolRuntime::doMagicOnlyRebolCanDo() {
   std::cout << "REBOL MAGIC!\n";
}



RebolRuntime::~RebolRuntime () {
    //OS_Call_Device(RDI_STDIO, RDC_CLOSE);
    OS_QUIT_DEVICES(0);
}

} // end namespace ren


