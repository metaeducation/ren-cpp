//
// rebol-os-lib-table.cpp
//
// This oddity is a workaround for the fact that Rebol seems to have never
// intended to to be able to ship its runtime to be "cracked open" and used
// in the way this binding is doing.  The idea that *anyone* would ever
// know (for instance) what the internal integer numbers were behind the
// types was a scary concept.
//
// It is a scary concept, but that doesn't mean there aren't times someone
// might want to do it.  Like when implementing a binding of this nature,
// for instance...
//
// Regardless, the various extension models for Rebol materialized as REBX,
// and the "host kit"...through which a host provided Rebol as a library
// a grab bag of services the interpreter used.  Internally Rebol would
// manage services and evaluation, but not actually know anything about the
// I/O mechanisms and speak totally abstractly.  In fact, even allocating
// and freeing memory dynamically were considered a service.
//
// Of course, a fixed length table of C function pointers is a rather
// old-school and "ioctl"-style way of thinking about that.  Moving on...
// this is to work around a specifically ornery header file called
// "host-lib.h".  You won't find it in the include directory on GitHub
// because it's a generated file.  It was cryptic to figure out and made it
// very hard to do what RenCpp needed to have done.
//
// It defines an important structure called REBOL_HOST_LIB...and it doesn't
// do so with any kind of conditionality (or include guards...).  So you can't
// run through it with one set of #defines and then run through it again with
// another; it may only be included once in a translation unit.
//
// After that declaration are some conditional definitions which do one of
// two things.  If the OS_LIB_TABLE preprocessor definition is set, then it
// will define an instance of a static variable Host_Lib_Init, which is a
// "Host library function vector table".  As we know, defining variables in
// header files creates problems...you can't include that file more than once
// in your project.
//
// So this file wraps up a bit of hard-to-piece together inclusions to be the
// single inclusion for when OS_LIB_TABLE is defined.
//
// There are two other modes.  If instead REB_DEF is set, then you will get
// some definitions like:
//
//    extern void OS_Free(void *mem);
//
// If neither are set, then you get some preprocessor defines like:
//
//    extern REBOL_HOST_LIB *Host_Lib;
//
//    #define OS_FREE(a) Host_Lib->os_free(a)
//
// My goal was to come up with a set of incantations that would build without
// changing the source repository of Rebol.  These are the incantations it
// took, and hopefully I've put any curious people on the road toward
// figuring it out more deeply in the future if they have interest.
//
// The actual initialization of the table is done in rebol-hooks.cpp

extern "C" {

#include <stdlib.h> // size_t

#define REB_DEF
#include "rebol/src/include/reb-c.h"
#include "rebol/src/include/sys-deci.h"
#include "rebol/src/include/reb-defs.h"
#include "rebol/src/include/reb-device.h"
#include "rebol/src/include/reb-event.h"
#include "rebol/src/include/sys-value.h"
#include "rebol/src/include/reb-filereq.h"
#undef REB_DEF


//
// It so happens that the makefiles for Rebol define -fno-common on OS/X.
// As a consequence, the linker winds up with multiple symbols somehow.
// So only define the Host_Lib table at this point if not on OS/X.  Sigh.
//

#ifndef TO_OSXI

#define OS_LIB_TABLE
#include "rebol/src/include/host-lib.h"

#endif
}
