Rebol [
]

remold: does [do make error! "remold deprecated, use SOURCE REDUCE"]

reform: does [do make error! "reform deprecated, use TO-STRING REDUCE"]

repend: does [do make error! "repend deprecated, use APPEND REDUCE"]

;-- We want to always use construction syntax
;-- Should it take /INDENT with an option of a string to use to indent
;-- with, or maybe /FORMAT for a forward-looking dialect of potentially
;-- supported options?  Could there be a switch saying supported options
;-- are required vs. not?

source: func [
    "Generate a Rebol-loadable source string from a value."
    value [any-type!] "The value to generate source for"
    /only {For a block value, mold only its contents, no outer []}
    /flat "No indentation"
] [
    case [
        all [only flat] [
            :system/contexts/lib/mold/flat/only value
        ]

        only [
            :system/contexts/lib/mold/only value
        ]

        flat [
            :system/contexts/lib/mold/flat value
        ]

        true [
            :system/contexts/lib/mold value
        ]
    ]
]

mold: does [do make error! "mold deprecated, use SOURCE"]
