//
// This file is modeled after Rebol's dev-stdio.c
//
// The Rebol "host kit" has a special handler for dealing with standard IO
// that is separate from the interaction with the files.  Because the C++
// iostream model already has the cin and cout implemented, we take out the
// handling and just use that; adding the ability to hook into it providing
// a custom ostream and istream...
//

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#if !defined TO_WINDOWS
    #include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <stdexcept>

#include "rencpp/rebol.hpp"
#include "rencpp/engine.hpp"

#include "common.hpp"


#define SF_DEV_NULL 31        // local flag to mark NULL device


extern REBDEV *Devices[];





/***********************************************************************
**
*/    static DEVICE_CMD Quit_IO(REBREQ *dr)
/*
***********************************************************************/
{
    REBDEV *dev = (REBDEV*)dr; // just to keep compiler happy above

    dev->flags &= ~cast(unsigned int, RDF_OPEN);
    return DR_DONE;
}


/***********************************************************************
**
*/    static DEVICE_CMD Open_IO(REBREQ *req)
/*
***********************************************************************/
{
    REBDEV *dev;

    dev = Devices[req->device];

    // Avoid opening the console twice (compare dev and req flags):
    if (dev->flags & RDF_OPEN) {
        // Device was opened earlier as null, so req must have that flag:
        if (dev->flags & SF_DEV_NULL)
            req->modes |= RDM_NULL;
        req->flags |= RRF_OPEN;
        return DR_DONE; // Do not do it again
    }

    if (req->modes & RDM_NULL)
        dev->flags|= SF_DEV_NULL;

    req->flags |= RRF_OPEN;
    dev->flags |= RDF_OPEN;

    return DR_DONE;
}


/***********************************************************************
**
*/    static DEVICE_CMD Close_IO(REBREQ *req)
/*
 ***********************************************************************/
{
    REBDEV *dev = Devices[req->device];

    req->flags &= ~cast(unsigned int, RRF_OPEN);

    return DR_DONE;
}


/***********************************************************************
**
*/    static DEVICE_CMD Write_IO(REBREQ *req)
/*
**        Low level "raw" standard output function.
**
**        Allowed to restrict the write to a max OS buffer size.
**
**        Returns the number of chars written.
**
***********************************************************************/
{
    if (req->modes & RDM_NULL) {
        req->actual = req->length;
        return DR_DONE;
    }

    std::ostream & os = ren::Engine::runFinder().getOutputStream();

    os.write(
        reinterpret_cast<char*>(req->common.data),
        static_cast<std::streamsize>(req->length) // Clang build needs cast
    );

    // knowing about a partial write would require using tellp() and comparing
    // which is both unreliable and not available on stdout anyway
    //
    //    http://stackoverflow.com/a/14238640/211160

    if (!os) {
        req->error = 1020;
        return DR_ERROR;
    }

    // For now we flush on every write.  It is inefficient, but it's not
    // clear what would be done about it otherwise if you are trying to read
    // the output in a console.  Perhaps an iostream class can be
    // self-flushing based on a timer, so if it hasn't been flushed for a
    // second it will?

    //if (GET_FLAG(req->flags, RRF_FLUSH)) {
        os.flush();
    //}

    // old code could theoretically tell you when you had partial output;
    // that's not really part of the ostream interface for write.  What
    // could you do about partial output to stdout anyway?

    req->actual = req->length;

    return DR_DONE;
}


/***********************************************************************
**
*/    static DEVICE_CMD Read_IO(REBREQ *req)
/*
**        Low level "raw" standard input function.
**
**        The request buffer must be long enough to hold result.
**
**        Result is NOT terminated (the actual field has length.)
**
***********************************************************************/
{
    u32 length = req->length;

    if (req->modes & RDM_NULL) {
        req->common.data[0] = 0;
        return DR_DONE;
    }

    req->actual = 0;

    std::istream & is = ren::Engine::runFinder().getInputStream();

    // There is a std::string equivalent for getline that doesn't require
    // a buffer length, but we go with the version that takes a buffer
    // length for now.  The only way to get the length of that is with
    // strlen, however.

    is.getline(
        reinterpret_cast<char*>(req->common.data),
        static_cast<std::streamsize>(req->length) // Clang needs this cast
    );

// C is *not* C++.  Ren-C is a C codebase, and its defines and macros may
// tread on C++ and that is how it is.  Prohibiting the use of "fail" as
// a method name is a liability, and Ren-C++ internally has the job of
// insulating all the C madness from clients.  Here's that at work.
#undef fail
    if (is.fail()) {
        req->error = 1020;
        return DR_ERROR;
    }

    req->actual = LEN_BYTES(req->common.data);

    return DR_DONE;
}


/***********************************************************************
**
*/    static DEVICE_CMD Open_Echo(REBREQ *)
/*
**        Open a file for low-level console echo (output).
**
***********************************************************************/
{
    throw std::runtime_error(
        "echo stdin and stdout to file not supported by binding"
        " in a direct fashion, you have to create a stream aggregator"
        " object that does it if you want that feature."
    );

    return DR_DONE;
}


/***********************************************************************
**
**    Command Dispatch Table (RDC_ enum order)
**
***********************************************************************/

static DEVICE_CMD_FUNC Dev_Cmds[RDC_MAX] =
{
    0,    // init
    Quit_IO,
    Open_IO,
    Close_IO,
    Read_IO,
    Write_IO,
    0,    // poll
    0,    // connect
    0,    // query
    0,    // modify
    Open_Echo,    // CREATE used for opening echo file
};

DEFINE_DEV(Dev_StdIO, "Standard IO", 1, Dev_Cmds, RDC_MAX, sizeof(REBREQ));
