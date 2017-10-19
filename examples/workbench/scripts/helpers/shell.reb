Rebol [
    Title: {Ren Garden Shell Dialect Helpers}

    Description: {
        One of Ren Garden's features is to allow interaction with persistent
        shell processes.  It is implemented as a "command-line dialect",
        so you can write things like:

            shell [FOO: *.cpp]

        ...and under the hood, what will actually be sent to the shell
        process (e.g. with UNIX /bin/sh) will look like:

            export FOO="*.cpp"

        The implementation of the process spawning logistics and data
        piping is done with [QProcess](http://doc.qt.io/qt-5/qprocess.html)
        and implemented on the C++ side.  However, how the dialect input is
        translated from a Ren block into a string for the shell is delegated 
        to the routines in this script.

        Please update the file in the docs directory if you change the
        behavior of the script.
    }

    Homepage: https://github.com/hostilefork/rencpp/blob/develop/examples/workbench/docs/shell.md

    License: 'BSD
]


block-to-shell-strings: function [arg [block!] windows [logic!]] [

    ; The simplest idea for what to do here would be to just run
    ; a COMPOSE/DEEP and then convert the result to a string.  But
    ; then you get situations where if escaped Rebol code resulted
    ; in a FILE!, it would still have the % sign in front of it.

    ; So for now, just a one-level deep parentheses substitution
    ; where we can treat the result of the substitution differently
    ; than if it hadn't been substituted

    ; set words are used for setting environment variables, and
    ; get words are used for retrieving them

    result: make block! 1
    str: make string! (5 * length arg)

    for-next arg [
        case [
            group? arg/1 [
                evaluated: do arg/1
                append str form evaluated
            ]

            set-word? arg/1 [
                append str unspaced [
                    windows ?? {set} !! {export}
                    space
                    spelling-of arg/1 {=}
                    not windows ?? {"}
                    form either group? arg/2 [do arg/2] [arg/2]
                    not windows ?? {"}
                ]
                arg: next arg
            ]

            get-word? arg/1 [
                append str unspaced [
                    windows ?? {%} !! {$}
                    spelling-of arg/1
                    windows ?? {%}
                ]
            ]

            block? arg/1 [
                ; A formality issue... should a shell dialect have to either
                ; be all block elements or no block elements?  We allow the
                ; flip for the moment but end the previous command; the
                ; implementation leaves a trailing space in that case ATM

                unless empty? str [
                    append result str
                ]

                append result block-to-shell-strings arg/1 windows

                str: make string! (5 * length arg/1)
            ]
        ] else [
            append str form arg/1
        ]

        unless any [block? arg/1 | last? arg] [
            append str space
        ]
    ]

    unless empty? str [
        append result str
    ]

    result
]


; The shell helper must return a block of strings that will be passed to
; the shell as sequential commands.

shell-dialect-to-strings: function [
    'arg [<opt> word! block! string!]
    windows [logic!]
][
    result: make block! 1

    switch type-of arg [
        _ [
            fail "Altering command processor state...soon"
        ]

        :word! [
            append result (form arg)
        ]

        :string! [
            append result (arg)
        ]

        :block! [
            result: block-to-shell-strings arg windows
        ]
    ]

    result
]
