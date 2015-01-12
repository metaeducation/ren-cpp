> **NOTE:** The Ren Garden project emerged from a "usage sample" to show how
> to write a functioning console using RenCpp, which could evaluate expressions
> and cancel their evaluation.  As testing evolved it quickly grew into
> something more promising.
>
> For early experimenters it is still being kept in the RenCpp project to ease
> building and versioning concerns. But please do not report Ren Garden feature
> ideas or bugs to the RenCpp repository, and instead use:
>
> https://github.com/metaeducation/ren-garden/issues
>
> The code will be moved into that repository at the appropriate time.

# Ren Garden

Ren Garden is a pre-release demo of a novel console for working with Rebol and
Red code evaluation, which uses widgets from the Qt5 toolkit.

Under the hood, the console is implemented as a QTextEdit; and it has
many features including:

* Evaluations run on a thread separate from the GUI, allowing for
responsiveness and cancellation

* Switching between single and multi-line editing, with a respectable suite
of multi-line editing features

* Browsing of previous input while a command is running, with terminal
heuristics to keep you in control of the positioning or advancing in lockstep
with command output

* Ability to undo an evaluation's output and return the console to the state
of input prior to the evaluation, including the cursor positioning

* Dynamic addition of watch variables and expressions through a WATCH dialect

* Management of a persistent command shell process which can be controlled via
a SHELL dialect

* ...and much more...


### Novelty

While many seemingly-similar initiatives like [iPython][1] do exist, what
differentiates Ren Garden is largely a matter of implementation methodology.
It derives a peculiar set of advantages from the uncompromising C++11 binding
"RenCpp"--itself standing on the shoulders of the unique design of Rebol
and Red.

So central to the purpose of Ren Garden is to be a challenging test case for
the RenCpp binding.  It's an exploration of what value using Rebol/Red from
inside C++ can offer (and vice-versa).

[1]: http://ipython.org/ipython-doc/dev/interactive/qtconsole.html


### Building

Building Ren Garden is currently covered in the RenCpp build instructions.

For assistance in building the project, please stop by
[Rebol and Red chat](http://rebolsource.net/go/chat-faq) or leave a note on
the issue database.


### License

RenCpp is under the liberal Boost software license.  However, Ren Garden is
being developed as a GPLv3 project.
