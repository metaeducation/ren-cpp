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

help: function [
    "Help dialect and interactive help shell"
    'word [any-type!]
    /meta
    
] [
    if unset? get/any 'word [
        console :help
        end
    ]

    if meta and word = 'banner [
        print trim/auto {
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
        end
    ]

    if meta and word = 'prompt [
        return "help"
    ]

    if all [
        word? :word 
        not value? :word
    ] [
        word: mold :word
    ]

    if all [
        doc
        word? :word
        any [any-function? get :word datatype? get :word]
    ] [
        item: form :word
        either any-function? get :word [
            foreach [a b] [
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
        browse join tmp [item ".html"]
    ]

    if any [string? :word all [word? :word datatype? get :word]] [
        if all [word? :word datatype? get :word] [
            value: spec-of get :word
            print [
                mold :word "is a datatype" newline
                "It is defined as" either find "aeiou" first value/title ["an"] ["a"] value/title newline
                "It is of the general type" value/type newline
            ]
        ]
        if any [:word = 'unset! not value? :word] [exit]
        types: dump-obj/match lib :word
        sort types
        if not empty? types [
            print ["Found these related words:" newline types]
            end
        ]
        if all [word? :word datatype? get :word] [
            print ["No values defined for" word]
            end
        ]
        print ["No information on" word]
        end
    ]

    type-name: function [value] [
        value: mold type? :value
        clear back tail value
        join either find "aeiou" first value ["an "] ["a "] value
    ]

    if not any [word? :word path? :word] [
        print [mold :word "is" type-name :word]
        end
    ]

    either path? :word [
        if any [
            error? set/any 'value try [get :word]
            not value? 'value
        ] [
            print ["No information on" word "(path has no value)"]
            end
        ]
    ] [
        value: get :word
    ]

    unless any-function? :value [
        print [
            uppercase mold word "is" type-name :value "of value: "
            either any [object? value port? value] [print "" dump-obj value] [mold :value]
        ]
        end
    ]

    ; It's a function, so print out the usage information

    print "USAGE:"
    print/only tab

    args: words-of :value
    clear find args /local
    either op? :value [
        print [args/1 word args/2]
    ] [
        print [uppercase mold word args]
    ]

    print/only [
        newline 
        "DESCRIPTION:" newline
        tab any [title-of :value "(undocumented)"] newline
        tab uppercase mold word { is } type-name :value { value.}
    ]

    unless args: find spec-of :value any-word! [exit]
    clear find args /local
    print-args: func [label list /extra /local str] [
        if empty? list [end]
        print label
        each arg list [
            str: combine [tab arg/1]
            if all [extra word? arg/1] [insert str tab]
            if arg/2 [append append str " -- " arg/2]
            if all [arg/3 not refinement? arg/1] [
                repend str [" (" arg/3 ")"]
            ]
            print str
        ]
    ]

    use [argl refl ref b v] [
        argl: copy []
        refl: copy []
        ref: b: v: none
        parse args [
            any [string! | block!]
            any [
                set word [refinement! (ref: true) | any-word!]
                (append/only either ref [refl] [argl] b: reduce [word none none])
                any [set v block! (b/3: v) | set v string! (b/2: v)]
            ]
        ]
        print-args "^/ARGUMENTS:" argl
        print-args/extra "^/REFINEMENTS:" refl
    ]
    end
]

