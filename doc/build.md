Building RenCpp
===============

This tutorial is still incomplete but it aims to explain at best how to build
RenCpp on different systems.

Be aware that although RenCpp was originally an initiative instigated by the
[Red Project](http://red-lang.org/contributions.html), it turned out to be
possible to abstract its design so it could also be used with Rebol. Due to
Rebol's C heritage, it was more feasible to make the first `ren::runtime` be
based on it for working out the design.

*(@HostileFork adds: Do not confuse "feasible" with "easy".)*

Hence at this time, RenCpp requires you to download and build Rebol from
source. Object files from that build are then borrowed from
`rebol/make/objs/*.o`, and statically linked against the C++ files. This
produces the RenCpp library...and adds empirically about 100K to
the half megabyte Rebol codebase (in release builds).

Red support will be announced for preview as soon as it can be.


Windows
-------

RenCpp should build with reasonably up-to-date versions of MinGW and Clang.
It relies on some tricky C++11 features, so it's highly unlikely that RenCpp
will build with Microsoft Visual Studio. At the time of writing, MSVC still
does not support (for instance) constructor inheritance.

The simplest way to build RenCpp is to use CMake. These instructions will
assume you are doing so and using MinGW, so you will need:

* [A recent version of MinGW][1]
(e.g. GCC 4.8.2+ with POSIX threads works.  GCC 4.7.2 is too old.)

* [A fairly recent version of CMake][2]
(2.8+)

* [The source code for Rebol][4]

* [A binary of the Rebol.exe interpreter][5]

* [The source code for RenCpp][6]

If you don't have a version of MinGW recent enough and you also want to build
the *"Ren Garden"* GUI console demo, then you'll have to download the Qt
binaries with the corresponding MinGW version:

* [Qt 5.4+ if you also want to build Ren Garden][3]
(choose the MinGW binaries in "Other Downloads")


**Building Rebol**

To build RenCpp, you will need to build Rebol first. Git clone it from the
source (or you can choose "Download ZIP").

*(Note: While 64-bit patched versions of Rebol exist, the "official" repository
has not accepted those pull requests. Hence it only compiles and runs correctly
in 32-bits mode. MinGW-w64 allows 32-bits builds, and you can also stick the
flag `-m32` onto the compiler options.)*

The Rebol build process includes preprocessing steps that are--themselves--
Rebol scripts. So download an interpreter, put it in the `make` subdirectory
of Rebol's source code, and rename it `r3-make.exe`. (If you are using a shell
other than CMD.EXE, then you might have to simply name it `r3-make`).

Next step is to tell Rebol to build a makefile for use with MinGW. So in the
`make` subdirectory of Rebol's source code, run the following lines:

    make make OS_ID=0.3.1
    make prep
    make

Now you should have a new subdirectory `objs` full of `.o` files, as well as an
executable named `r3.exe` in the make directory. Great! You have built Rebol!

Next step: RenCpp.


**Building RenCpp**

Download the source code of RenCpp and put it in a directory next to the one
that you named `rebol`. Having these two directories side by side is the
default assumption of the build process. If you put rebol somewhere else, you
will need to specify `-DRUNTIME_PATH=/wherever/you/put/rebol-source`.

Make sure that you have `cmake`, `make` and the other MinGW executables in your
PATH.  (MinGW may have named its make `mingw32-make` if `make` alone is not
working for you.)  Then open a console in RenCpp's main directory and type the
following instructions:

    cmake -G"MinGW Makefiles" -DRUNTIME=rebol
    make

If all goes well, you should get a `libRenCpp.a` as well as some executables
in the examples directory to try out.


**Building Ren Garden**

If you want to build Ren Garden along with RenCpp, then CMake will need to be
able to find where Qt installed its CMake "package finders".  This is generally
done via the environment variable CMAKE_PREFIX_PATH. So if you installed Qt
in `C:\Qt\5.4`, then be sure something along these lines is set in whatever
environment you are invoking cmake from:

    CMAKE_PREFIX_PATH=C:\Qt\5.4\mingw491_32\lib\cmake

*(Note: Be sure to check to make sure this directory is actually there, and
make the necessary adjustments if not.)*

Once that's ready, type the following instructions:

    cmake -G"MinGW Makefiles" -DRUNTIME=rebol -DCLASSLIB_QT=1 -DGARDEN=yes
    make

If everything went smoothly, you should have an executable! However, the
required DLLs for the C++ runtime, pthreads, and Qt will likely not be in your
path. So odds are you'll get several DLL not found errors when you try to run.

Should all else fail, you can go through the DLL errors one by one and put
those alongside the executable. The DLLs should be somewhere like:

    C:\Qt\5.4\mingw491_32\bin

If adding that to your system path does the trick, you might be happy with
that. A less invasive option is to create a small batch file to launch the
process that does it only for that session:

    echo off
    set PATH=%PATH%;C:\Qt\5.4\mingw491_32\bin
    start workbench.exe


Linux
-----

The initial development of RenCpp and Ren Garden was on a Kubuntu installation
with Qt Creator.  Using Kubuntu and Qt Creator is about the easiest situation
you can find.  Even if you don't run a Linux host, you might consider setting
up a virtual machine with a recent installation:

* [Kubuntu](http://www.kubuntu.org/)

* [Qt](http://www.qt.io/download-open-source/)

Note that you'll need something newer than Debian "wheezy" stable to build it,
as that only comes with GCC 4.7.2.  You can--however--upgrade an older
installation using the .deb package management (e.g. for "jessie")

* [Upgrading GCC on Debian](http://stackoverflow.com/a/26167100/211160)

To get CMake, all you should need to run is:

    sudo apt-get install cmake

You will still have to install and build Rebol.  (TBD...)

Once you've built Rebol, in Qt Creator open the CMakeLists.txt file.  It
should offer you a command line to type your settings into.  To build RenCpp
and Ren Garden type:

    -DCMAKE_BUILD_TYPE=Debug -DRUNTIME=rebol -DCLASSLIB_QT=1 -DGARDEN=yes

(Adaptations for other Linuxes TBD...)



OS/X
----

While it is possible to install GCC on an OS/X machine, it involves some extra
steps...so we will assume you are using Clang and/or XCode.

Certainly no Clang older than 3.1 can build RenCpp (which introduced lambda
support, and was released in May 2012).  So check your version with:

   clang -v

**Without Ren Garden**

Follow the steps for building Rebol and placing the directories as in the
Windows instructions, but with a different OS_ID.  There is also an issue
at the time of writing which needs to be investigated which requires a small
edit to the Rebol source:

    make make OS_ID=0.2.5
    make prep
    make

As on other platforms, if you're on a 64 bit system you will (for the moment)
have to build as 32-bit:

   cmake -DCMAKE_CXX_FLAGS=-m32 -DRUNTIME=rebol

**With Ren Garden**

On other platforms, 32-bit Qt libraries are distributed pre-built by the
Qt project.  For Mac only 64-bit binaries are available.  Ren Garden has been
built successfully using a 64-bit patched Rebol, and turnkey instructions for
this process are still pending.  See [this chat log][7].


Support
-------

Should these steps not work, the best place to get real-time help is via
[Rebol and Red chat][8]. There is also the [RenCpp Issue Tracker][9].
on GitHub.

Good luck! And if you find yourself frustrated by any aspect of this process,
remember that's why Rebol and Red exist in the first place: *to fight software
complexity*.  But it's easier to get traction in that fight if you can sneak
your way in with subversive integration tools like RenCpp... :-)


[1]: http://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/
[2]: http://www.cmake.org/download/
[3]: http://www.qt.io/download-open-source/
[4]: https://github.com/rebol/rebol
[5]: http://rebolsource.net/
[6]: https://github.com/hostilefork/rencpp
[7]: http://chat.stackoverflow.com/transcript/message/20951003#20951003
[8]: http://rebolsource.net/go/chat-faq
[9]: https://github.com/hostilefork/rencpp/issues
