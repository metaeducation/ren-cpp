Rebol [
    Title: {Dialected Help Function}

    Description: {
        The original Rebol HELP was a bit like PROBE, in that if you
        if you passed it any random value it would tell you what it
        knew about that value.  Unfortunately, this limits its
        service as a dialect because...
 
            help [some dialect code]

        ...will come back and tell you that [some dialect code] is a 
        block with three elements.  :-/

        This is the beginnings of help-as-dialect, which can take
        over in a mode very much like SHELL or similar.  What all
        it might be willing to do is a subject for research, but
        various ideas like querying or submitting to the bug 
        database could be interesting.

        This includes some fixes in any case; before asking for
        HELP on a function would source it if you didn't quote the
        word.
    }
]

;--
;-- This was the old help string to be borrowed from as appropriate.
;--
{
^-^-^-Use HELP or ? to see built-in info:

^-^-^-^-help insert
^-^-^-^-? insert

^-^-^-To search within the system, use quotes:

^-^-^-^-? "insert"

^-^-^-To browse online web documents:

^-^-^-^-help/doc insert

^-^-^-To view words and values of a context or object:

^-^-^-^-? lib    - the runtime library
^-^-^-^-? self   - your user context
^-^-^-^-? system - the system object
^-^-^-^-? system/options - special settings

^-^-^-To see all words of a specific datatype:

^-^-^-^-? native!
^-^-^-^-? function!
^-^-^-^-? datatype!

^-^-^-Other debug functions:

^-^-^-^-docs - open browser to web documentation
^-^-^-^-?? - display a variable and its value
^-^-^-^-probe - print a value (molded)
^-^-^-^-source func - show source code of func
^-^-^-^-trace - trace evaluation steps
^-^-^-^-what - show a list of known functions
^-^-^-^-why? - explain more about last error (via web)

^-^-^-Other information:

^-^-^-^-chat - open DevBase developer forum/BBS
^-^-^-^-docs - open DocBase document wiki website
^-^-^-^-bugs - open CureCore bug database website
^-^-^-^-demo - run demo launcher (from rebol.com)
^-^-^-^-about - see general product info
^-^-^-^-upgrade - check for newer versions
^-^-^-^-changes - show changes for recent version
^-^-^-^-install - install (when applicable)
^-^-^-^-license - show user license
^-^-^-^-usage - program cmd line options
^-^-}

help: function [
    "Help dialect and interactive help system"
    'arg [any-type!]
    /meta
] [ 
    if unset? get/any 'arg [
        console :help
        return void 
    ]

    if meta [
        if arg = 'banner [
            print/only [

    {Entering HELP dialect mode for the command line.} newline
    {(To exit this mode, hit ESCAPE twice quickly.)} newline
    {More information coming on this development soon...} newline

            ]
            return void 
        ]

        if arg = 'prompt [
            return "help"
        ]

        ;-- Ignore any meta requests we don't understand (room for expansion)

        return void 
    ]

    if all [
        word? :arg
        not value? :arg
    ] [
        arg: to-string :arg
    ]

    ; Online documentation search request... could technically support this
    ; as a refinement but then you couldn't "get at it" from inside the
    ; dialect.  Could have a meta switch that would automatically fetch the
    ; online help instead of the built-in, when available:
    ;
    ;     help/meta [online on]
    ;
    ; The larger question of whether Ren Garden wants to get "thicker" by
    ; including a WebKit build to put HTML inline is an undecided issue,
    ; but it would be a no-brainer if the actual console itself were
    ; switched to a web view instead of a QTextEdit

    doc: false

    if all [
        doc
        word? :arg
        any [any-function? get :arg datatype? get :arg]
    ] [
        item: to-string :arg
        either any-function? get :arg [
            every [a b] [
                "!" "-ex"
                "?" "-q"
                "*" "-mul"
                "+" "-plu"
                "/" "-div"
                "=" "-eq"
                "<" "-lt"
                ">" "-gt"
            ] [replace/all item a b]
            tmp: http://www.rebol.com/r3/docs/functions/
        ] [
            tmp: http://www.rebol.com/r3/docs/datatypes/
            remove back tail item
        ]
        browse combine [tmp item ".html"]
    ]

    type-name: function [value] [
        str: to-string type-of :value
        clear back tail str ;-- remove exclamation point from type
        combine [(either find "aeiou" first str ["an"] ["a"]) space str]
    ]

; Needs to be improved and done properly in the dialect.. what about
; searching with PARSE, e.g.
;
;     help [match ["ap" to end]]
;
; That could give back a list of all the entries with ap as a prefix, for
; instance... (not that it couldn't be supported the old way too)

;        types: dump-obj/match lib :arg
;        sort types
;        if not empty? types [
;            print ["Found these related words:" newline types]
;            return void
;        ]

    case [
        all [word? :arg datatype? get :arg] [
            value: spec-of get :arg
            print [
                to-string :arg "is a datatype" newline

                "It is defined as"
                either find "aeiou" first value/title ["an"] ["a"]
                value/title newline

                "It is of the general type" value/type newline
            ]
        ]

        word? :arg [
            value: get :arg
        ]

        any [:arg = 'unset! not value? :arg] [
            return void
        ]

        all [word? :arg datatype? get :arg] [
            print ["No values defined for" arg]
            return void
        ]

        path? :arg [
            if any [
                error? set/any 'value try [get :arg]
                not value? 'value
            ] [
                print [{No information on} :arg "(path has no value)"]
                return void
            ]
        ]

        true [
            value: :arg
        ]
    ]

    unless any-function? :value [
        print [
            if word? :arg [
                uppercase to-string :arg "is"
            ]
            type-name :value "of value: "
            either any [object? value port? value] [
                [newline dump-obj value]
            ] [
                to-string :value
            ]
        ]
        return void 
    ]

    ; It's a function, so print out the usage information

    print "USAGE:"
    print/only tab

    parameters: words-of :value
    clear find parameters /local
    either op? :value [
        print [
            parameters/1
            either any-function? :arg ["(...)"] [to-string :arg]
            parameters/2
        ]
    ] [
        print [
            either any-function? :arg ["(...)"] [uppercase to-string :arg]
            to-string parameters
        ]
    ]

    print/only [
        newline 

        "DESCRIPTION:" newline

        tab any [title-of :value "(undocumented)"] newline

        tab
        {Function sub-type is} space
        type-name :value newline
    ]

    unless parameters: find spec-of :value any-word! [return void]
    clear find parameters /local
    print-parameters: function [label list /extra] [
        if empty? list [return void]
        print label
        every param list [
            print/only [
                if all [extra word? param/1] [tab]
                tab to-string param/1
                if param/2 [
                    [space "--" space to-string param/2]
                ]
                if all [
                    param/3
                    not refinement? param/1
                ] [
                    [space "(" to-string param/3 ")"]
                ]
                newline
            ]
        ]
    ]

    argl: copy []
    refl: copy []
    ref: b: v: none
    parse parameters [
        any [string! | block!]
        any [
            set item [refinement! (ref: true) | any-word!]
            (
                b: reduce [item none none]
                append/only either ref [refl] [argl] b
            )
            any [set v block! (b/3: v) | set v string! (b/2: v)]
        ]
    ]
    print-parameters "^/ARGUMENTS:" argl
    print-parameters/extra "^/REFINEMENTS:" refl
    void 
]

