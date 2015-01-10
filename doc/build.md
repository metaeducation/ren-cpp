Building RenCpp
===============

This tutorial is still incomplete but it aims to explain at best how to build
RenCpp on different systems.

Linux
-----

TODO

Windows
-------

It's highly unlikely that RenCpp will build with Microsoft Visual Studio since its
C++11 support is still too weak and RenCpp relies on some tricky C++11 things. It
should build with MinGW and Clang (no tutorial yet) though. This tutorial does not
explain how to build RenCpp with Red either; it focuses on Rebol.

The simplest way to build RenCpp is to use CMake. To build RenCpp on Windows with
CMake, you will need:

* [A recent version of MinGW][1] (4.9+ with POSIX threads)
* [A fairly recent version of CMake][2] (2.8+)
* [Qt 5.4+ if you also want to build Ren Garden][3] (choose the MinGW binaries in "Other Downloads")
* [The source code of Rebol][4]
* [The Rebol interpreter][5]
* [The source code of RenCpp][6]

Note that if you don't have a version of MinGW recent enough and you also want to
build Ren Garden, then you better download the Qt binaries with the corresponding
MinGW version (it's in Qt's download options).

To build RenCpp, you will need to build Rebol first. Get it from the source (you
can choose "Download ZIP") and put it somewhere on your computer. Make sure that
the directory's name is `rebol`; this will be essential to run RenCpp. Now, to build
Rebol, you will need the Rebol interpreter, so you may as well get the latest build.
Download the interpreter, put it in the `make` subdirectory of Rebol's source code
and rename it `r3-make.exe` (or try to simply name it `r3-make` if you have it tells
you that it is what it needs).

Next step, we need to configure the makefile to be able to use it with MinGW. Open
a console in the `make` subdirectory of Rebol's source code and run the following
lines:

```
make make OS_ID=0.3.1
make prep
make
```

Now you should have a new subdirectory `objs` full of `.o` files as well as an
executable named `r3.exe`. Great! You have built Rebol! Next step: RenCpp. So,
download the source code of RenCpp and put it in a directory next to the one
that you named `rebol`. Having these two directories side by side is essential
if you want the following steps to work.

Make sure that you have `cmake`, `make` and the other MinGW executables in your
PATH, then open a console in RenCpp's main directory and type the following
instructions:

```
cmake -G"MinGW Makefiles" -DRUNTIME=rebol
make
```

If you want to build Ren Garden along with RenCpp, type the following instructions
instead:

```
cmake -G"MinGW Makefiles" -DRUNTIME=rebol -DGARDEN=1 -DCLASSLIB_QT=1
make
```

You may need to tell CMake where to find the files required to use Qt5Core and
Qt5Widgets. Adding the following line in `CMakeLists.txt` might be enough (I
couldn't get it to work with environment variables yet, ut if you manage to do
so, then please use environment variables instead):

```
set(CMAKE_PREFIX_PATH "C:\\Qt\\Qt5.4.0\\5.4\\mingw491_32\\lib\\cmake\\Qt5Core" "C:\\Qt\\Qt5.4.0\\5.4\\mingw491_32\\lib\\cmake\\Qt5Widgets")
```

Of course, the absolute links above are examples and may not correspond to the
version of Qt or MinGW that you use, but you get the idea. Now, if everything
went smoothly, you should be done!



[1]: http://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/
[2]: http://www.cmake.org/download/
[3]: http://www.qt.io/download-open-source/
[4]: https://github.com/rebol/rebol
[5]: http://rebolsource.net/
[6]: https://github.com/hostilefork/rencpp