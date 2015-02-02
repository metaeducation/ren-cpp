Rebol [
    Title: {Updated PRINT to use COMBINE and add /ONLY}
]

prin: does [make error! "prin is now accomplished via print/only"]

print: function [
    {Print a value to standard output, using COMBINE dialect if a block}

    value [any-type!] {The value to print}

    /only {Print the value with no spaces if COMBINEd, and no newline added}

    /with {Use a delimiter or delimiter function (will be COMBINEd if block)}
        delimiter

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
    case [
        unset? :value [
            ;-- ignore it...
        ]

        block? value [
            prin combine/with/part value (
                case [with [delimiter] only [none] true [space]]
            ) (
                either part [limit] [tail value]
            )
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

