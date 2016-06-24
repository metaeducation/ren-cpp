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
        less desirable than JOIN.  Though JOIN was the original requested
        name, it was backed off after significant resistance defending
        its existing definition:

            join: func [
                "Concatenates values."
                value "Base value"
                rest "AnyType or block of values"
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

    unless set? 'delimiter [delimiter: _]

    unless any-function? :delimiter [
        unless block? delimiter [
            delimiter: reduce [delimiter]
        ]

        delimiter: func [depth [integer!]] compose/only/deep [
            combine (delimiter)
        ]
    ]

    depth: any [:depth 1]

    if all [
        part
        block? limit
    ][
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
                (not blank? temp) or (block? out)
            ][
                out: append out temp
            ]
        ][
            needs-delimiter: true? with
        ]
    ]


    ; Do evaluation of the block until a non-blank evaluation result
    ; is found... the limit is hit...or end of the input is reached.

    original: block

    while [
        all [
            not tail? block
            any [
                blank? part
                either integer? limit [
                    0 <= limit: limit - 1
                ][
                    block != limit
                ]
            ]
        ]
    ][
        ; Note: DO/NEXT should be able to take `quote block:` in order for
        ; FUNCTION to pick it up as a local.  It does not currently, and
        ; in this case that is okay since block is a parameter and won't
        ; leak as a global

        value: either safe [
            either any [word? block/1 path? block/1] [
                get/any first back (block: next block)
            ][
                first back (block: next block)
            ]
        ][
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
            ; Ignore voids (precedent: ANY, ALL, COMPOSE)

            not set? 'value []


            ; Skip all blanks.  This is suggested for COMPOSE as well:
            ;
            ;     http://curecode.org/rebol3/ticket.rsp?id=2198

            blank? :value []


            ; If a function or PAREN! are returned, then evaluate it at
            ; the same nesting level (unless /SAFE is chosen)
            ;
            ; This does mean you can get an infinite loop, for instance by
            ; writing something like:
            ;
            ;    foo: quote (foo)
            ;    combine [foo]
            ;
            ; Of course, in general Rebol offers no guarantees against
            ; infinite loops.  So this experiment is just a test to see if
            ; it's more useful to allow the feature than to disallow it; and
            ; see if creative usages arise making it worth not prohibiting.
            ; The existence of a /SAFE option made this seem less insane.

            any [
                any-function? :value
                group? value
            ][
                if safe [
                    do make error! {
                        COMBINE evaluation cannot re-evaluate result if /SAFE
                    }
                ]
                pre-delimit ;; overwrites temp!
                out: combine/with/into/level reduce [value] :delimiter out depth
            ]


            ; Recursing on blocks is foundational to COMBINE and allowing
            ; nested sequences; like building nested rules in PARSE

            block? value [
                pre-delimit ;; overwrites temp!
                out: combine/with/into/level value :delimiter out depth + 1
            ]


            ; If all else fails, run a TO-STRING conversion on the item.
            ;
            ; Initial versions of COMBINE would not automatically TO-STRING a
            ; word!, set-word!, path!, etc.  But once COMBINE became used in
            ; PRINT, previous applications suggested that these cases were
            ; valuable for doing debug-related or reflective processing.  The
            ; automatic choice to use TO-STRING (previously FORM) vs.
            ; something more MOLD-like has a consequence, e.g.:
            ;
            ;     print [quote Hello: %{/filename with/spaces/in it}]
            ;
            ; Gives you:
            ;
            ;     Hello: /filename with/spaces/in it
            ;
            ; Which is clearly not LOADable, and puts responsibility of
            ; delimiting things which may contain spaces upon the user.
            ; You may MOLD to avoid this, of course:
            ;
            ;     print [quote Hello: mold %{/filename with/spaces in it}]
            ;
            ; Then you will get:
            ;
            ;     Hello: %{/filename with/spaces in it}
            ;
            ; However, the default is to use TO-STRING and not raise an error,
            ; by popular request.

            true [
                pre-delimit ;; overwrites temp!
                out: append out form value
            ]
        ]
    ]

    return either into [out] [head out]
]
