Rebol [
    Title: {Combine}

    Description: {
       The COMBINE dialect is intended to assist with the common task of creating
       a merged string series out of component Rebol values.  Its
       goal is to be friendlier than REJOIN, and to hopefully become the
       behavior backing PRINT.

       Currently in a proposal period, and there are questions about whether the
       same dialect can be meaningful for blocks or not.

           http://blog.hostilefork.com/combine-alternative-rebol-red-rejoin/ 
    }
]


combine: func [
    block [block!]
    /with "Add delimiter between values (will be COMBINEd if a block)"
        delimiter
    /into
        out [any-string!]
    /local
        needs-delimiter pre-delimit value temp
    ; hidden way of passing depth after /local, review...
    /level depth
] [
    ;-- No good heuristic for string size yet
    unless into [
        out: make string! 10
    ]

    unless any-function? :delimiter [
        unless block? delimiter [
            delimiter: reduce [delimiter]
        ]
        delimiter: func [depth [integer!]] compose/only/deep [
            combine (delimiter)
        ]
    ]

    unless depth [
        depth: 1
    ]

    needs-delimiter: false
    pre-delimit: does [
        either needs-delimiter [
            set/any 'temp delimiter depth
            if all [
                value? 'temp
                (not none? temp) or (block? out)
            ] [ 
                out: append out temp
            ]
        ] [
            needs-delimiter: true? with
        ]
    ]

    ;-- Do evaluation of the block until a non-none evaluation result
    ;-- is found... or the end of the input is reached.
    while [not tail? block] [
        set/any 'value do/next block 'block

        ;-- Blocks are substituted in evaluation, like the recursive nature
        ;-- of parse rules.

        case [
            unset? :value [
                ;-- Ignore unset! (precedent: any, all, compose)
            ]

            any-function? :value [
                do make error! "Evaluation in COMBINE gave function/closure"
            ]

            block? value [
                pre-delimit
                out: combine/with/into/level value :delimiter out depth + 1
            ]

            ; This is an idea that was not met with much enthusiasm, which was
            ; to allow COMBINE ['X] to mean the same as COMBINE [MOLD X]
            ;any [
            ;    word? value
            ;    path? value
            ;] [
            ;    pre-delimit ;-- overwrites temp!
            ;    temp: get value
            ;    out: append out (mold :temp)
            ;]

            ; It's a controversial question as to whether or not a literal
            ; word should mold out as its spelling.  The idea that words
            ; don't cover the full spectrum of strings is something that
            ; got stuck in my head that words shouldn't "leak".  So I was
            ; very surprised to see that they did, and if you used a word
            ; selection out of a file path as FILE/SOME-WORD then it would
            ; append the spelling of SOME-WORD to the file.  That turned
            ; the idea on its head to where leakage of words might be okay,
            ; along with the idea of liberalizing what strings could be 
            ; used as the spelling of words to anything via construction
            ; syntax.  So pursuant to that I'm trying to ease up so that
            ; if evaluation winds up with a word value, e.g. held in 
            ; a variable or returned from a function then that is 
            ; printable.  But set words, get words, and paths are too
            ; "alive" and should be molded.  Hmmm.  FORM is a better
            ; word than MOLD.  If TO-STRING could take the responsibility
            ; of FORM then MOLD could take FORM.  Enough tangent.

            any [
                word? value
            ] [
                pre-delimit ;-- overwrites temp!
                out: append out (to-string value)
            ]

            ; Another idea that seemed good at first but later came back not
            ; seeming so coherent...use of an otherwise dead type to 
            ; suppress delimiting.  So:
            ;
            ;     >> combine/with ["A" "B" /+ "C"] "."
            ;     == "A.BC"
            ;  
            ; This was particularly ugly when the pieces being joined were
            ; file paths and had slashes in them.  But the concept may be  
            ; worth implementing another way so that the delimiter-generating
            ; function can have a first crack at processing values?

            ;refinement? value [
            ;    case [
            ;        value = /+ [
            ;            needs-delimiter: false
            ;        ]
            ;
            ;        true [
            ;            do make error! "COMBINE refinement other than /+ used"
            ;        ]
            ;    ]
            ;]

            any-block? value [
                ;-- all other block types as *results* of evaluations throw
                ;-- errors for the moment.  (It's legal to use PAREN! in the
                ;-- COMBINE, but a function invocation that returns a PAREN!
                ;-- will not recursively iterate the way BLOCK! does) 
                do make error! "Evaluation in COMBINE gave non-block! or path! block"
            ]

            any-word? value [
                ;-- currently this throws errors on words if that's what an
                ;-- *evaluation* produces.  Theoretically these could be
                ;-- given behaviors in the dialect, but the potential for
                ;-- bugs probably outweighs the value (of converting implicitly
                ;-- to a string or trying to run an evaluation of a non-block)
                do make error! "Evaluation in COMBINE gave symbolic word"
            ]

            none? value [
                ;-- Skip all nones
            ]

            true [
                pre-delimit
                out: append out (system/contexts/lib/form :value)
            ]
        ]
    ]
    either into [out] [head out]
]

