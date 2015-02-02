One of Ren Garden's features is to allow interaction with persistent shell 
processes.  The implementation of the process spawning logistics and 
data piping is done in a spirit similar to Tcl's EXPECT:

http://en.wikipedia.org/wiki/Expect

It is the beginning of implementing such an idea for Ren Garden.  What
largely differentiates it is choice of tools (Rebol/Red, C++11, and Qt's 
[QProcess](http://doc.qt.io/qt-5/qprocess.html)).  

How the dialect input is translated from a Ren block into a string for the 
shell is delegated to the routines in a Rebol helper script: 

https://github.com/hostilefork/rencpp/blob/develop/examples/workbench/scripts/helpers/shell.reb

As the dialect is in its earliest days of implementation, there are many
open questions (including the question of whether to go ahead and make it
built upon a more generalized EXPECT dialect). Yet this is the beginnings 
of a working document describing what has been done so far:


### LITERAL STRINGS ###

If you give the shell a string literal, it will be passed uninterpreted:

    shell [{echo $FOO}]

The dialect continues this pattern for any string literals which are found, 
which will be separated by a single space from their neighboring content.  

Additionally, any legal words are currently left untouched by the dialect,
They are simply converted to their spellings:

    shell [echo {$FOO}]

Should you be lucky enough that the entirety of your shell command can be
expressed as words, you won't need any string escaping:

    shell [ls -alF]

> **Note:** For almost every choice in the dialect design, it will overlap 
> with some pre-existing behavior.  In curly braces case, it is used
> for expansion and grouping:
>
> http://stackoverflow.com/questions/8748831/bash-why-do-we-need-curly-braces-in-variables
>
> All cases of wanting to use such features of a shell can work by passing
> literal strings, and it's easy to nest curly brace literals.  The goal
> is to have the dialecting features allow augmentations, especially shifting
> more of the  "metaprogramming" to the more pleasing devices of Rebol and Red.


### ENVIRONMENT VARIABLES ###

Environment variables can be accessed with literal strings in the usual
way (as in the `{echo $FOO}` above).  Yet as $FOO would be an "invalid
money value" as a REN literal, GET-WORD! is used instead.

    shell [{echo} :FOO]

That would transmit to the shell `echo $FOO`.  SET-WORD! also has special
processing...and since `*.cpp` is a legal word we can write:

    shell [FOO: *.cpp]

Under the hood, that would send to a shell (if it were based on UNIX /bin/sh)
a string that would look like:

    export FOO="*.cpp"

> **Note:** For pre-existing behavior in colon's case, bash uses it as a 
> sort of "quoting operator":
> 
> http://superuser.com/questions/423980/colon-command-for-bash
>
> `{:}` would be an adequate substitute, modulo implicit spacing concerns.


### EMBEDDED EVALUATION ###

Shell blocks are currently implicitly preprocessed by COMPOSE prior to
running.  So all of the parenthesized code will run prior to any of the
shell code:

    shell [ls (reverse {Fla-})]

That would be seen by the shell as `ls -alF`.

> **Note:** The shells use parentheses for their own evaluation, but 
> that can be done with {(} and {)} if you really needed it.


### SEPARATING COMMANDS ###

Because Ren code is not newline-sensitive outside of strings, a first idea
was to puncutate multiple commands during one SHELL invocation using inner
blocks:

    shell [
        [{echo} :FOO]
        [ls :FOO]
    ] 

It's up for debate on this idea (as with many) if there might be a better
application for the construct, as you could just as easily write:

    shell [
        {echo} :FOO
        {;}
        ls :FOO
    ] 

Probably done better as:

    return-codes: copy []

    each cmd [
        [{echo} :FOO]
        [ls :FOO]
    ] [
        append return-codes (shell cmd)
    ]

More interesting feature ideas might come up, though.  Maybe there would be
some way to ask the shell processor to run commands in parallel, and return
a block of exit codes once they all finish:

    shell [
        parallel 
        [ ... ]
        [ ... ]
        [ ... ]
    ]

Other less ambitious ideas could be when you want a group of things to be
combined without spacing:

    shell [
        {;}
        echo [foo :BAZ bar]
    ]

If `:BAZ` were to produce `{$BAZ}`, this would give the ability to make
things like `echo foo{$BAZ}bar`.


> **Note:** `[` as well as `[[` are actual commands in UNIX, believe it
> or not:
>
>    $ which [
>    /bin/[
>
> The idea again being that if you needed that, {[} and {[[} would be
> adequate substitutes.


### EVERYTHING ELSE ###

So far, everything else turns into its TO-STRING form (as implemented in
Ren Garden's TO-STRING variant).  So if you want to use a file path you
can write `%/dir/file.txt`.  This would integrate more nicely with SHELL
if NewPath existed:

http://blog.hostilefork.com/new-path-debate-rebol-red/


### THE FUTURE...? ###

Lots of work to do.  Good news for the Rebol/Red crowd is that a lot of 
it doesn't require C++ expertise.  Discussions at:

http://rebolsource.net/go/chat-faq

