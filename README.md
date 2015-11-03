# RenCpp

This is an experimental C++11 Language binding for the [Rebol and Red][1]
programming languages.  They are unique in use of [definitional scoping][2]
and a boilerplate-free syntax to act as *"the most freeform programming
languages ever invented"*.

The homepage for this project will be:

http://rencpp.hostilefork.com/

Though build instructions are maintained on the GitHub wiki:

https://github.com/metaeducation/ren-cpp/wiki

The code is not "released" in any kind of formal fashion.  It is only on GitHub
for review by collaborators in the initial design.  However, when released it
will be targeting the Boost Software License 1.0 (BSL), so consider the work
in progress to be under that license as well.

The repository also currently includes code for the [Ren Garden][2]
cross-platform "IDC" (or "Interactive Development Console"), based on RenCpp.
Both are kept in the same repository only for convenience, and Ren Garden
has a separate issue tracker on GitHub and will be moved.  Ren Garden's C++/Qt
codebase is under the GPLv3 license, with its "helper" routines (currently
written in Rebol) under the BSD license.

---
[1]: http://blog.hostilefork.com/why-rebol-red-parse-cool/
[2]: http://stackoverflow.com/a/33469555/211160
[2]: https://www.youtube.com/watch?v=0exDvv5WEv4
