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

    ; Unicode test (and promotion of @HostileFork's philosophy...)

    copyright: "<i><b>Ren Garden</b> is © 2015 MetÆducation, GPL 3 License</i>"


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

        every arg [
            case [
                paren? arg/1 [
                    evaluated: do arg/1
                    append str to-string evaluated
                ]

                set-word? arg/1 [
                    append str combine [
                        either windows [{set}] [{export}]
                        space spelling-of arg/1 {=}
                        either windows [
                            to-string either paren? arg/2 [do arg/2] [arg/2]
                        ] [
                            [
                                {"}
                                either paren? arg/2 [do arg/2] [arg/2]
                                {"}
                            ]
                        ]
                    ]
                    arg: next arg
                ]

                get-word? arg/1 [
                    append str combine [
                        either windows [
                            [{%} spelling-of arg/1 {%}]
                        ] [
                            [{$} spelling-of arg/1]
                        ]
                    ]
                ]

                block? arg/1 [
                    ; A formality issue... should a shell dialect
                    ; have to either be all block elements or no
                    ; block elements?  We allow the flip for the
                    ; moment but end the previous command; the implementation
                    ; leaves a trailing space in that case as written now

                    unless empty? str [
                        append result str
                    ]

                    append result block-to-shell-strings arg/1 windows

                    str: make string! (5 * length arg/1)
                ]

                true [
                    append str to-string arg/1
                ]
            ]

            unless any [block? arg/1  last? arg] [
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
        'arg [word! block! string! unset!]
        windows [logic!]
    ] [
        result: make block! 1

        case [
            unset? arg [
                do make error! "Altering command processor state...soon"
            ]

            word? arg [
                append result (to-string arg)
            ]

            string? arg [
                append result (arg)
            ]

            block? arg [
                result: block-to-shell-strings arg windows
            ]
        ]

        result
    ]
]
