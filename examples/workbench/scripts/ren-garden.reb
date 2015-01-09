Rebol [
    Title: {Ren Garden Helpers module}

    Description: {
        Typing Rebol source code into C++ as string literals is 
        techincally possible but not all that convenient.  While 
        it's good to test the ergonomics of doing so, (and they're
        actually pretty good), being able to code one's Rebol/Red in
        separate files as some hooks specific to the app and use
        them can be more pleasant

        Note this is built into the executable by being put into
        the .qrc file, and accessed via the resource mechanism.
        A rebuild will be necessary to pick up any changes.
   }
]

ren-garden: context [
    copyright: "<i><b>Ren Garden</b> is © 2015 MetÆducation, GPL 3 License</i>"

    ;-- more hooks to come... but there's an example to get started.
]
