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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "rencpp/rebol.hpp"


#define SF_DEV_NULL 31		// local flag to mark NULL device


extern REBDEV *Devices[];





/***********************************************************************
**
*/	static DEVICE_CMD Quit_IO(REBREQ *dr)
/*
***********************************************************************/
{
	REBDEV *dev = (REBDEV*)dr; // just to keep compiler happy above

    CLR_FLAG(dev->flags, RDF_OPEN);
	return DR_DONE;
}


/***********************************************************************
**
*/	static DEVICE_CMD Open_IO(REBREQ *req)
/*
***********************************************************************/
{
	REBDEV *dev;

	dev = Devices[req->device];

	// Avoid opening the console twice (compare dev and req flags):
	if (GET_FLAG(dev->flags, RDF_OPEN)) {
		// Device was opened earlier as null, so req must have that flag:
        if (GET_FLAG(dev->flags, SF_DEV_NULL))
			SET_FLAG(req->modes, RDM_NULL);
		SET_FLAG(req->flags, RRF_OPEN);
		return DR_DONE; // Do not do it again
	}

	if (!GET_FLAG(req->modes, RDM_NULL)) {
	}
    else {
        int temp = dev->flags;
        SET_FLAG(temp, SF_DEV_NULL);
        dev->flags = temp;
    }

	SET_FLAG(req->flags, RRF_OPEN);
	SET_FLAG(dev->flags, RDF_OPEN);

	return DR_DONE;
}


/***********************************************************************
**
*/	static DEVICE_CMD Close_IO(REBREQ *req)
/*
 ***********************************************************************/
{
	REBDEV *dev = Devices[req->device];

	CLR_FLAG(req->flags, RRF_OPEN);

	return DR_DONE;
}


/***********************************************************************
**
*/	static DEVICE_CMD Write_IO(REBREQ *req)
/*
**		Low level "raw" standard output function.
**
**		Allowed to restrict the write to a max OS buffer size.
**
**		Returns the number of chars written.
**
***********************************************************************/
{
	if (GET_FLAG(req->modes, RDM_NULL)) {
		req->actual = req->length;
		return DR_DONE;
	}

    std::ostream & os = rebol::runtime.getOutputStream();

    os.write(reinterpret_cast<char*>(req->data), req->length);

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
*/	static DEVICE_CMD Read_IO(REBREQ *req)
/*
**		Low level "raw" standard input function.
**
**		The request buffer must be long enough to hold result.
**
**		Result is NOT terminated (the actual field has length.)
**
***********************************************************************/
{
    if (GET_FLAG(req->modes, RDM_NULL)) {
		req->data[0] = 0;
		return DR_DONE;
	}

	req->actual = 0;

    std::istream & is = rebol::runtime.getInputStream();

    is.read(reinterpret_cast<char*>(req->data), req->length);

    if (is.fail()) {
        req->error = 1020;
        return DR_ERROR;
    }

    req->actual = is.gcount();

	return DR_DONE;
}


/***********************************************************************
**
*/	static DEVICE_CMD Open_Echo(REBREQ *)
/*
**		Open a file for low-level console echo (output).
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
**	Command Dispatch Table (RDC_ enum order)
**
***********************************************************************/

static DEVICE_CMD_FUNC Dev_Cmds[RDC_MAX] =
{
	0,	// init
	Quit_IO,
	Open_IO,
	Close_IO,
	Read_IO,
	Write_IO,
	0,	// poll
	0,	// connect
	0,	// query
	0,	// modify
	Open_Echo,	// CREATE used for opening echo file
};

DEFINE_DEV(Dev_StdIO, const_cast<char*>("Standard IO"), 1, Dev_Cmds, RDC_MAX, 0);
