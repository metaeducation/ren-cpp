Building Ren/C++
================

This tutorial is still incomplete but it aims to explain at best how to build
Ren/C++ on different systems.

Be aware that although Ren/C++ was originally an initiative instigated by the
[Red Project](http://www.red-lang.org/p/contributions.html), it turned out to
be possible to abstract its design so it could also be used with Rebol.  Due to
Rebol's C heritage, it was more feasible to make the first `ren::runtime` be
based on it for working out the design.

*(@HostileFork adds: Do not confuse "feasible" with "easy".)*

Hence at this time, Ren/C++ requires you to download and build Rebol from
source.  Initially it would work against either the original open source
codebase at GitHub's rebol/rebol (as well as the community build found at
rebolsource/r3).  However, with the genesis of the Ren/C project it has begun
building only against that fork.

As a data point on size, Ren/C++ adds adds empirically about 100K to a
standalone Rebol build...and has not been particularly optimized for size.

Red support will be announced for preview as soon as it can be.


Windows
-------

Ren/C++ should build with reasonably up-to-date versions of MinGW and Clang.
It relies on some tricky C++11 features, so it's highly unlikely that Ren/C++
will build with Microsoft Visual Studio. At the time of writing, MSVC still
does not support (for instance) constructor inheritance.

The simplest way to build Ren/C++ is to use CMake. These instructions will
assume you are doing so and using MinGW, so you will need:

* [A recent version of MinGW][1]
(e.g. GCC 4.8.2+ with POSIX threads works.  GCC 4.7.2 is too old.)

* [A fairly recent version of CMake][2]
(2.8+)

* [The source code for the Ren/C fork of Rebol][4]

* [A binary of the Rebol.exe interpreter][5]

* [The source code for Ren/C++][6]

If you don't have a version of MinGW recent enough and you also want to build
the *"Ren Garden"* GUI console demo, then you'll have to download the Qt
binaries with the corresponding MinGW version:

* [Qt 5.4+ if you also want to build Ren Garden][3]
(choose the MinGW binaries in "Other Downloads")


**Building Ren/C to use in Ren/C++**

To build Ren/C++, you will need to build Ren/C first. Git clone it from the
source (or you can choose "Download ZIP"):

    git clone https://github.com/metaeducation/ren-c rebol

*(Note: Depending on the likelihood of Rebol taking back the changes in
Ren/C or not, the directory used may start saying "ren-c" instead of Rebol.
Yet there are some signs that Ren/C will be adopted back into the Core of
the various Rebol builds.  So we'll still say it's a "rebol build" for now.)*

The Rebol build process includes preprocessing steps that are--themselves--
Rebol scripts. So download an interpreter, put it in the `make` subdirectory
of Rebol's source code, and rename it `r3-make.exe`. (If you are using a shell
other than CMD.EXE, then you might have to simply name it `r3-make`).

Next step is to tell Rebol to build a makefile for use with MinGW. So in the
`make` subdirectory of Rebol's source code, run this line for a 32-bit build:

    make -f makefile.boot OS_ID=0.3.1

If you will be doing a 64-bit build, instead type:

    make -f makefile.boot OS_ID=0.3.2

*(Note: MinGW-w64 allows 32-bits builds, and you can also stick the flag
`-m32` onto the compiler options.)*

Now you should have a new subdirectory `objs` full of `.o` files, as well as an
executable named `r3.exe` in the make directory. Great! You have built Rebol!

Next step: Ren/C++.


**Building Ren/C++**

Download the source code of Ren/C++ and put it in a directory next to the one
that you named `rebol`. Having these two directories side by side is the
default assumption of the build process. If you put rebol somewhere else, you
will need to specify `-DRUNTIME_PATH=/wherever/you/put/rebol-source`.

Make sure that you have `cmake`, `make` and the other MinGW executables in your
PATH.  (MinGW may have named its make `mingw32-make` if `make` alone is not
working for you.)  Then open a console in Ren/C++'s main directory and type the
following instructions:

    cmake -G"MinGW Makefiles" -DRUNTIME=rebol
    make

If all goes well, you should get a `libRenCpp.a` as well as some executables
in the examples directory to try out.


**Building Ren Garden**

If you want to build Ren Garden along with Ren/C++, then CMake will need to be
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

The initial development of Ren/C++ and Ren Garden was on a Kubuntu installation
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
should offer you a command line to type your settings into.  To build Ren/C++
and Ren Garden type:

    -DCMAKE_BUILD_TYPE=Debug -DRUNTIME=rebol -DCLASSLIB_QT=1 -DGARDEN=yes

(Adaptations for other Linuxes TBD...)

**Kubuntu 14.10 (32bit)**

Here are my steps for running on a local VM under virtual box.

First up the resolution is not right running under virtualbox so we need
to fix that:

    sudo apt-get install virtualbox-guest-dkms

Grab qt and install it (I had a few problems with the download, but it worked
in the end):

    wget http://download.qt-project.org/official_releases/online_installers/qt-opensource-linux-x86-online.run
    chmod 755 qt-opensource-linux-x86-online.run
    ./qt-opensource-linux-x86-online.run

and install a few dependencies

    sudo apt-get install cmake -y
    sudo apt-get install ia32-libs -y
    sudo apt-get install lib32z1 -y
    sudo apt-get install libc6-dev-i386 -y

and a few more ...

    sudo apt-get install build-essential -y
    sudo apt-get install mesa-common-dev -y
    sudo apt-get install libglu1-mesa-dev -y
    sudo apt-get install git -y

Now setup a directory for Ren/C++:

    mkdir ren
    cd ren

Download the Ren/C forked source of Rebol into a directory called "rebol":

    git clone https://github.com/metaeducation/ren-c rebol
    cd rebol/make/

Grab a pre-built Rebol executable to run the make:

    wget http://rebolsource.net/downloads/linux-x86/r3-g25033f8
    chmod 755 r3-g25033f8 
    cp r3-g25033f8 r3-make

Now build Rebol.  For 32-bit Linux:

    make -f makefile.boot OS_ID=0.4.4

If you are building for 64-bit:

    make -f makefile.boot OS_ID=0.4.40
    
This should have successfully built Rebol.

Now change dir back to the ren dir, setup the path to Qt for cmake and grab
the source for Ren/C++ into the ren-cpp directory:

    export CMAKE_PREFIX_PATH=~/Qt/5.4/gcc_64/lib/cmake
    git clone https://github.com/metaeducation/ren-cpp

Change dir to ren-cpp and build it (note: if you installed your Qt elsewhere
be sure to update the path...)

    cd ren-cpp
    mkdir build
    cd build
    cmake .. -G "Unix Makefiles" -DRUNTIME=rebol -DGARDEN=1 -DCLASSLIB_QT=1 -DCMAKE_PREFIX_PATH=~/Qt/5.4/gcc/lib/cmake
    make

If the make is successful then try to launch Ren Garden:

    cd examples/workbench
    ./workbench
    

OS X
----

**Prerequisites**

* [Install Xcode via App store](https://developer.apple.com/xcode/)
* [Download Qt](http://download.qt.io/official_releases/qt/5.4/5.4.0/qt-opensource-mac-x64-clang-5.4.0.dmg)

Qt setup wizard (see later) complains if Xcode is not installed.  It may well
work without it?  All the other steps work fine just with the
[Command Line Tools](https://developer.apple.com/library/ios/technotes/tn2339/_index.html#//apple_ref/doc/uid/DTS40014588-CH1-DOWNLOADING_COMMAND_LINE_TOOLS_IS_NOT_AVAILABLE_IN_XCODE_FOR_OS_X_10_9__HOW_CAN_I_INSTALL_THEM_ON_MY_MACHINE_).

* You can use alternative GNU/GCC development environment instead of above.
These can be installed via package tools like Homebrew or Macports.  These
package tools could also be used to install CMake and QT.  *(Note: You will
need Clang older than version 3.1 to use this.)*

**Build with Ren Garden**

The following steps are self-contained and are as simple as I can make them!  

So from a terminal session:

    mkdir RenGarden

This will be the working directory.  You can give it any name and place it
anyway on your hard disk.  Examples that follow will use this nomenclature.

**Installing QT and CMake**

Double click the on the downloaded QT dmg package and follow the setup wizard:

* **Installation folder** - Set this to be your working directory
(ie. RenGarden folder)

* **Select Components** - Fine to leave this as is.  However it can be trimmed
down to just **clang 64-bit** (and **Qt Creator**)

* Click through rest of steps

*(Note: Installing Qt Creator would allow you participate in the QT development
of the project perhaps a little more easily, depending on which Qt Creator
features wind up being used, for instance the Form and GUI designer tool.)*

You should now have QT installed in our working directory.  Next steps:

    cd RenGarden
    curl http://www.cmake.org/files/v3.1/cmake-3.1.1-Darwin-x86_64.tar.gz | tar -xz
    ls -F

And you should see (something like) this in your working directory.

    Qt5.4.0/
    cmake-3.1.1-Darwin-x86_64/

We now have QT and CMake ready for us to continue.   

**Build Ren/C and Ren/C++**

    git clone https://github.com/metaeducation/ren-c rebol
    git clone https://github.com/metaeducation/ren-cpp
    cd rebol
    curl http://rebolsource.net/downloads/osx-x86/r3-g25033f8 > ./make/r3-make

We are now ready to compile Rebol:

    cd make
    chmod +x ./r3-make
    make -f makefile.boot OS_ID=0.2.40

If all went well we now have a brand new Rebol executable:

    file r3

So lets build Ren/C++

    cd ../../ren-cpp
    PATH=$PATH:../Qt5.4.0/5.4/clang_64/bin ../cmake-3.1.1-Darwin-x86_64/CMake.app/Contents/bin/cmake -DRUNTIME=rebol -DCLASSLIB_QT=1 -DGARDEN=yes
    make

If CMake gets to 100% then we are all good!  Test Ren/C++ with following step:

    ./tests/test-rencpp

If no errors are reported then you're ready to open RenGarden.

    open examples/workbench/

This will open a Finder window on desktop.  Look for **workbench**, double
click and enjoy :)


Support
-------

Should these steps not work, the best place to get real-time help is via
[Rebol and Red chat][8]. There is also the [Ren/C++ Issue Tracker][9].
on GitHub.

Good luck! And if you find yourself frustrated by any aspect of this process,
remember that's why Rebol and Red exist in the first place: *to fight software
complexity*.  But it's easier to get traction in that fight if you can sneak
your way in with subversive integration tools like Ren/C++... :-)


[1]: http://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/
[2]: http://www.cmake.org/download/
[3]: http://www.qt.io/download-open-source/
[4]: https://github.com/metaeducation/ren-c
[5]: http://rebolsource.net/
[6]: https://github.com/metaeducation/ren-cpp
[7]: http://chat.stackoverflow.com/transcript/message/20951003#20951003
[8]: http://rebolsource.net/go/chat-faq
[9]: https://github.com/metaeducation/ren-cpp/issues
