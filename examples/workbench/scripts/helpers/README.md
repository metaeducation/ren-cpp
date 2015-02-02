This directory contains helper scripts in native Ren format for Ren Garden.  
They implement parts of the functionality which are easier to write (or 
maintain) than if they were inlined into the C++ source.

The more of the console's behavior that can be implemented in these scripts, 
the more easily they can be maintained by non-C++ programmers.  Pursuant to 
the goal of being used and understood outside of the context of being
called from a C++ binding, Rebol conventions are followed wherever possible
(such as 1-based indexing).

Note these files are embedded into the executable by being put into the 
.qrc file, and accessed via the resource mechanism.  That means a rebuild 
will be necessary to pick up any changes.  A more dynamic mechanism that
would allow reloading or updating these scripts without needing a new
executable is in the early stages of design.


### LICENSE ###

Ren Garden is a GPL project.  Yet these helper scripts are BSD-licensed, and
other non-C++-or-RenCpp-based consoles are free to borrow them.  The 
dialects and their design are welcome to be used in other Rebol/Red-based 
projects.

(At time of writing not particularly significant to say so.  But who knows--
this portion of the project might grow to be the majority of Ren Garden's
general logic.)


### A NOTE ABOUT RATIONALE ###

Entering Rebol and Red source code directly is possible with RenCpp, and 
is made as painless as the medium permits:

    Block rule {
        "thru {[}",
        "copy", some-cpp-variable, "to {]}",
        "to end"
    };

This is the only way to splice Ren values which have been wrapped as C++ 
objects directly into series constructions.  However, let's say you just 
want to inline some code that LOADs a value with no embedded C++ 
variables, e.g. a function:

    Function putInQuotes = {
        "function [foo bar] [ "
            "quoted: combine [{\"} foo bar {\"}] "
            "print quoted "
            "return quoted "
        "]"
    };

There are several inconveniences:

* **Different Escaping Rules** - String literals in C++ are victim to the 
usual legacy concerns and visual blight the C-family-language style is 
known for.  It's fortunate that Ren's alternate string form with curly 
braces can be embedded without necessitating two escapes for *every* string.  
Yet the backslashes do wind up making their appearance at times, and 
become all the more confusing when trying to read mixed-medium code.

* **Multi-Line Strings** - There are two ways to do multi-line strings in C++
and [neither is optimal](http://stackoverflow.com/a/1135862/211160).  If
If you terminate with a quote and then start with a quote on the next line,
they will be merged...but this requires remembering to leave a space on the 
end.  (In the above example, not putting the space after "print quoted" would 
lead to "print quotedreturn quoted").  Using backslashes is ugly and will
also lead to "fattening" the constant at runtime with wasted space for any
indentation.

* **Permanent Cost** - String constants embedded in the program in this way
are part of the constant pool and address space for the entire program run.
That is additional storage to keeping around the loaded representation.
The source code of helpers loaded from these files is fetched from disk
into memory, loaded, and then freed...keeping only the loaded representation.

* **Must Touch C++ to Edit** - Many contributors who could improve the
scripts would not be able (or willing) to do so if it involved editing C++
source files, compiling to make sure they didn't have syntax errors, etc.

So it's best to factor out plain source instead of inlining it, whenever 
the specific dynamism of RenCpp is not required.

