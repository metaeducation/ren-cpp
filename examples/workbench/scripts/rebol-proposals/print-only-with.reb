Rebol [
    Title: {Modernized PRINT using JOIN (a.k.a. COMBINE)}

    Description: {
        Once COMBINE was written, it soon became apparent that most of the
        time you are doing a PRINT of a block, you wanted the COMBINE
        semantics.  It was a bit tedious to write:

            print combine [ ... ]

        It's a little less ugly in Ren Garden, where COMBINE has taken the
        more sensible name of JOIN.  But still, using it under the hood
        seems a good idea considering that there aren't any really great
        definitions for what a block would PRINT as otherwise.  Leveraging
        the COMBINE spec is both well-defined and powerful.

        That meant exposing other properties of the COMBINE spec, like /WITH
        and (now /SAFE and /PART).  Under the hood, PRINT uses
        COMBINE/WITH [...] SPACE by default.

        Yet the other twist was to replace PRIN with PRINT/ONLY, which drops
        the space and the newline.  It's possible to reintroduce the spacing
        but not the newline by using:

            print/only/with [...] space

        And it's possible to drop the spacing but keep the newline:

            print/with [...] none

        It's the PRINT you never knew you've always wanted.  :-)
    }
]

prin: does [make error! "prin is now accomplished via PRINT/ONLY"]

print: function [
    {Print a value to standard output, using COMBINE dialect if a block}

    value [any-type!] {The value to print}

    /only {Print the value with no spaces if COMBINEd, and no newline added}

    /with {Use a delimiter or delimiter function (will be COMBINEd if block)}
        delimiter

    /safe "Do not perform function evaluations, only GET values"

    ; Having a /PART refinement is nice if you just want to do the combine
    ; on some sub-portion of a block and don't want to copy, but it's a
    ; slippery slope toward adding a /PART to every operation that is
    ; used on a series that defaults to going to its end (another almost
    ; always useful one is /REVERSE).  In the absence of an iterator
    ; abstraction, the question is how many routines actually need such
    ; a refinement vs. telling people to COPY.  Review.

    /part {Limits to a given length or position}
        limit [integer! series!] ;; COMBINE doesn't implement pair! yet
] [
    prin: :system/contexts/lib/prin

    ; Transform our refinements into arguments for COMBINE

    with-arg: case [with [delimiter] only [none] true [space]]
    part-arg: either part [limit] [tail value]

    case [
        unset? :value [
            ;-- ignore it...
        ]

        block? value [
            ;-- chaining proposal would make this oh-so-much-better
            prin either safe [
                combine/with/part/safe value with-arg part-arg
            ] [
                combine/with/part value with-arg part-arg
            ]
        ]

        string? value [
            prin value
        ]

        path? value [
            prin mold get value
        ]

        series? value [
            do make error! "Cannot print non-block!/string! series directly, use MOLD or lit-word"
        ]

        word? value [
            prin mold get value
        ]

        any-word? value [
            do make error! "Cannot print non-word! words directly, use MOLD"
        ]

        any-function? value [
            do make error! "Cannot print functions directly, use MOLD or lit-word"
        ]

        object? value [
            do make error! "Cannot print objects directly, use MOLD or lit-word"
        ]

        true [
            prin system/contexts/lib/form value
        ]
    ]

    unless only [
        prin newline
    ]

    system/contexts/lib/exit
]

