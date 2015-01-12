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


Linux
-----

TODO


Windows
-------

RenCpp should build with reasonably up-to-date versions of MinGW and Clang.
It relies on some tricky C++11 features, so it's highly unlikely that RenCpp
will build with Microsoft Visual Studio. At the time of writing, MSVC still
does not support (for instance) constructor inheritance.

The simplest way to build RenCpp is to use CMake. These instructions will
assume you are doing so and using MinGW, so you will need:

* [A recent version of MinGW][1]
(e.g. GCC 4.9+ with POSIX threads works)

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
PATH, then open a console in RenCpp's main directory and type the following
instructions:

    cmake -G"MinGW Makefiles" -DRUNTIME=rebol
    make

If all goes well, you should get a `libRenCpp.a` as well as some executables
in the examples directory to try out.


**Building Ren Garden**

If you want to build Ren Garden along with RenCpp, then instead of the above,
type the following instructions:

    cmake -G"MinGW Makefiles" -DRUNTIME=rebol -DCLASSLIB_QT=1 -DGARDEN=yes
    make

You will need to need to tell CMake where Qt installed its CMake "package
finders".  This is generally done via the environment variable
CMAKE_PREFIX_PATH. So if you installed Qt in `C:\Qt\5.4`, then make sure
something along these lines is set in whatever environment you are
invoking cmake from:

    CMAKE_PREFIX_PATH=C:\Qt\5.4\mingw491_32\lib\cmake

If that doesn't work for some reason, adding these lines to `CMakeLists.txt`
may be enough of a workaround to get the build to happen

    set(
        CMAKE_PREFIX_PATH
        "C:\\Qt\\5.4\\mingw491_32\\lib\\cmake\\Qt5Core"
        "C:\\Qt\\5.4\\mingw491_32\\lib\\cmake\\Qt5Widgets"
        "C:\\Qt\\5.4\\mingw491_32\\lib\\cmake\\Qt5Gui"
    )

*(Note: Of course, the absolute links above are examples, and may not
correspond to the version of Qt or MinGW that you use. So be sure to check
to make sure these directories are actually there, and make the necessary
adjustments if not.)*

Now, if everything went smoothly, you should have an executable! However, the
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


Support
-------

Should these steps not work, the best place to get real-time help is via
[Rebol and Red chat](http://rebolsource.net/go/chat-faq). There is also the
[RenCpp Issue Tracker](https://github.com/hostilefork/rencpp/issues) on GitHub.

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
