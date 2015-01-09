Rebol [
    Title: {Updated PRINT to use COMBINE and add /ONLY}
]

unset 'prin

print: function [value [any-type!] /only /with delimiter] [
    prin: :system/contexts/lib/prin
    case [
        unset? :value [
            ;-- ignore it...
        ] 

        block? value [
            prin combine/with value (case [with [delimiter] only [none] true [space]])
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

