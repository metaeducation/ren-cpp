Rebol [
    Title: {Character set generation functions}

    Description: {
        A longstanding complaint is the lack of in-the-box character sets,
        especially because it creates an unnecessary barrier to getting
        started writing common parse rules.  Generator functions which keep
        an on-demand cache of common character classes has the advantage of
        being able to use both refined and unrefined versions to return a
        character set, so things like LETTER and LETTER/UPPERCASE/LATIN or
        DIGIT and DIGIT/HEX can all evaluate to character sets without having
        to resort to global hyphenations like LETTER-UPPERCASE-LATIN (which
        has an ordering problem with LETTER-LATIN-UPPERCASE as well).  They
        can also cache the character sets on demand.  Unfortunately functions
        have longstandingly not been able to be used in PARSE.  The feature
        is controversial because if functions of arbitrary arity were allowed
        in PARSE it may become unreadable.  The solution is to only allow
        arity zero functions.

        As a language that accepts Unicode out of the box, inspiration should
        hopefully come from the Unicode character classes:

        http://www.fileformat.info/info/unicode/category/index.htm

        Note that Unicode charsets get very large, so the caching nature of
        these builders is important (vs. making all of them regardless of
        need).  However, it might be interesting for the caching to be able
        to return the value and release in a single operation.
    }
]

cached: func [
    {If value is not none, return it, otherwise evaluate the block}
    target [word! path!] ;-- object! ?  (GET supports it)
    cache-block [block!]
] [
    either not none? get target [get target] [set target do cache-block]
]


digit: function/with [
    {Digit caching character set generator (defaults to decimal?)}
    /binary
    /hex
    /decimal
    /uppercase
    /lowercase
] [
    case [
        all [uppercase lowercase] [
            do make error! "/uppercase and /lowercase used together"
        ]

        binary [
            hex [
                do make error! "/binary and /hex used together"
            ]

            cached '.binary [charset [#"0" #"1"]]
        ]

        hex [
            case [
                uppercase [
                    cached '.hex-uppercase [charset [#"0" - #"9" #"A" - #"F"]]
                ]

                lowercase [
                    cached '.hex-lowercase [charset [#"0" - #"9" #"a" - #"f"]]
                ]

                true [
                    cached '.hex [charset [#"0" - #"9" #"a" - #"f" #"A" - #"F"]]
                ]
            ]
        ]

        any [decimal true] [
            cached '.decimal [charset [#"0" - #"9"]]  
        ]
    ]
] [
    .binary: 
    .decimal:
    .hex: .hex-uppercase: .hex-lowercase:
    none
]



letter: function/with [
    {Letter caching character set generator (defaults to Unicode letter class)}
    /uppercase
    /lowercase
    /english
] [
    case [
        all [uppercase lowercase] [
            do make error! "/uppercase and /lowercase refinements used together"
        ]

        english [
            ;-- Is "english letters" better than "ASCII letters" for A-Za-z?
            ;-- http://en.wikipedia.org/wiki/English_alphabet
            case [
                uppercase [
                    cached '.english-uppercase [charset [#"A" - #"Z"]]
                ]

                lowercase [
                    cached '.english-lowercase [charset [#"a" - #"z"]]
                ]

                true [
                    cached '.english [charset [#"A" - #"Z" #"a" - #"z"]]
                ]
            ]
        ]

        ; Default should work with unicode.
        true [
            do make error! "/english refinement needed (need unicode? add it!)"
        ]
    ]
] [
    .english: .english-lowercase: .english-uppercase:
    none
]



whitespace: function/with [
    /ascii
] [
    case [
        ascii [
            cached '.ascii [charset reduce [tab space newline cr]]
        ]

        ; Default should work with unicode spaces, etc.
        true [
            do make error! "/ascii refinement needed (need unicode? add it!)"
        ]
    ]
] [
    .ascii:
    none
]