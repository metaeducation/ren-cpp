Rebol [
    Title: {JOIN Dialect (codename: COMBINE)}

    Description: {
        The COMBINE dialect is intended to assist with the common task of
        creating a merged string series out of component Rebol values.  Its
        goal is to be friendlier than REJOIN...and to be the engine that
        powers the PRINT primitive.

        Currently in a proposal period, and there are questions about whether
        the same dialect can be meaningful for blocks or not.

           http://blog.hostilefork.com/combine-alternative-rebol-red-rejoin/ 

        While the name COMBINE is not bad, it has several things making it
        less desirable than JOIN.  While JOIN was the original requested
        name, it was backed off after significant resistance defending
        its existing definition:

            join: func [
                "Concatenates values."
                value "Base value"
                rest "Value or block of values"
            ] [
                value: either series? :value [copy value] [form :value]
                repend value :rest
            ]

        Yet the usefulness of COMBINE has come to be so overwhelming that the
        Ren Garden project is going ahead with giving the name JOIN to it,
        in the defaults.  But the proposal is keeping the name COMBINE for
        usage in Rebol if it refuses to accept the change.  Thus, for the
        purposes of co-evolution it will continue to be called COMBINE in
        conversation.
    }
]


combine: function [
    {Produce a formatted string by recursively descending into a block.}

    block [block!]

    /with "Add delimiter between values (will be joined if a block)"
        delimiter

    /into "avoid intermediate by combining into existing string buffer"
        out [any-string!]

    ; Should /PART support a pair! ?  COPY/PART claims to, but does not
    ; seem to actually work...

    /part {Limits to a given length or position}
        limit [integer! block!]

    /safe "Do not perform function evaluations, only GET values"

    /level "Starting level to report when using /with a function"
        depth [integer!]
] [
    unless into [
        out: make string! 10 ;; No good heuristic for string size yet
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

    if all [
        part 
        block? limit
    ] [
        unless (head limit) = (head block) [
            do make error! {/PART limit series must be same as input series}
        ]
    ]

    needs-delimiter: false
    pre-delimit: does [
        either needs-delimiter [
            set/any quote temp: delimiter depth
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


    ; Do evaluation of the block until a non-none evaluation result
    ; is found... the limit is hit...or end of the input is reached.

    original: block

    while [
        all [
            not tail? block
            any [
                none? part
                either integer? limit [
                    0 <= limit: limit - 1
                ] [
                    block <> limit
                ]
            ]
        ] 
    ] [
        ; Note: DO/NEXT should be able to take `quote block:` in order for
        ; FUNCTION to pick it up as a local.  It does not currently, and
        ; in this case that is okay since block is a parameter and won't
        ; leak as a global
 
        set/any (quote value:) either safe [
            either any [word? block/1 path? block/1] [
                get/any first back (block: next block)
            ] [
                first back (block: next block)
            ]
        ] [
            do/next block 'block
        ]


        ; Major point of review for using a /PART-style refinement on
        ; a DO-like evaluator...what if your /PART location does not
        ; land on an evaluation boundary?  How would you stop at half
        ; an addition, e.g.:
        ;
        ;     do/part [1 + 2 3 + 4] 2
        ;
        ; A little sad to rule it out when many scenarios do have enough
        ; control to make use of what they need.  Restricting /PART to
        ; mechanical cases where they "can't fail" only causes people
        ; to use a COPY/PART and then a DO (or COMPOSE) on the extracted
        ; portion...often getting errors anyway.
        ;
        ; (One unusual case where a COPY/PART can avoid errors that a
        ; COMPOSE/PART wouldn't is when they remove things off the boundary
        ; that are infix operators...which can extend an evaluation
        ; when having it missing would terminate it cleanly.  This is more
        ; one of the many cases where permitting infix screws something
        ; up vs. an indictment of COMPOSE/PART.)
        ;
        ; In any case, as the first case of setting precedent for an
        ; evaluative /PART, the test behavior will be to see if you
        ; passed a point or nailed it.  TBD...


        ; Blocks are substituted in evaluation, like the recursive nature
        ; of parse rules.

        case [
            ; Ignore unset! (precedent: any, all, compose)

            unset? :value []


            ; Give no meaning to if the evaluation produces a function
            ; value as a product.

            any-function? :value [
                do make error! "Evaluation in COMBINE gave function/closure"
            ]


            ; Recursing on blocks is foundational to COMBINE and allowing
            ; nested sequences; like building nested rules in PARSE

            block? value [
                pre-delimit
                out: combine/with/into/level value :delimiter out depth + 1
            ]


            ; This is an idea that was not met with much enthusiasm, which was
            ; to allow COMBINE ['X] to mean the same as COMBINE [MOLD X]
            
            UNSET? ;; comment hack
            any [
                word? value
                path? value
            ] [
                pre-delimit ;-- overwrites temp!
                temp: get value
                out: append out (mold :temp)
            ]


            ; It's a controversial question as to whether or not ANY-WORD!
            ; should casually print out as its spelling.  The idea that words
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
                pre-delimit ;; overwrites temp!
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

            UNSET?
            refinement? value [
                case [
                    value = /+ [
                        needs-delimiter: false
                    ]
            
                    true [
                        do make error! "COMBINE refinement other than /+ used"
                    ]
                ]
            ]


            ; all other block types as *results* of evaluations throw
            ; errors for the moment.  (It's legal to use PAREN! in the
            ; COMBINE, but a function invocation that returns a PAREN!
            ; will not recursively iterate the way BLOCK! does) 

            any-block? value [
                do make error! {
                    Evaluation in COMBINE gave non-block! or path! block
                }
            ]


            ; currently this throws errors on words if that's what an
            ; *evaluation* produces.  Theoretically these could be
            ; given behaviors in the dialect, but the potential for
            ; bugs probably outweighs the value (of converting implicitly
            ; to a string or trying to run an evaluation of a non-block)

            any-word? value [
                do make error! "Evaluation in COMBINE gave symbolic word"
            ]


            ; Skip all nones

            none? value []


            ; If all else fails, then FORM the value

            true [
                pre-delimit
                out: append out system/contexts/lib/form :value
            ]
        ]
    ]

    return either into [out] [head out]
]
