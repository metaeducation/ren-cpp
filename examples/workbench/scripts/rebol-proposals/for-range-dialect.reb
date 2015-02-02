Rebol [
    Title: {Range dialect of FOR}

    Description: {
       Work in progress.
    }
]

; foreach is bad as a name but is "each" literate enough vs. for-each?  Seems
; to be pretty good.  A non-destructive /REVERSE is a very important option
; and needs to be added, but it's not possible to write good non-native
; wrappers due to a blocking bug ATM:
;
;    http://issue.cc/r3/539

each: :foreach
foreach: does [do make error! "foreach is now each"]


; this is a hard one to name, but a good rationale for every would be
; that it can be explained as "visiting every position" in the series.
; certainly foreach and forall were not as good.

every: :forall
forall: does [do make error! "forall is now every"]


; http://chat.stackoverflow.com/transcript/message/15915182#15915182
; http://issue.cc/r3/884

c-for: func [
    init [block!]
    test [block!]
    step [block!]
    body [block!]
    /local out
] [
    init: context init

    while bind/copy test init
        bind compose/deep [
           set/any 'out (to paren! body)
           (step)
        ] init

    get/any 'out
]


; makes a function you can call repeatedly that will give you
; items until eventually a none or unset comes along, based on
; a dialect.  This dialect has not been designed yet, so we
; just assume it's [X to Y] or [X thru Y]

generates: func [
    spec [block!]
    /local start finish
] [
    start: first spec
    finish: last spec
    if 'to = second spec [
        -- finish
    ]
    function/with [] [
       if all-done [
           return none
       ]
       temp: state
       if state = final [
           all-done: true
       ]
       ++ state
       return temp
    ] [
        state: start
        final: finish
        first-run: true
        all-done: false
    ]
]


for: func [
    'word
    spec [block!]
    body [block!]
    /local fun
] [
    fun: generates spec
    while [set/any word fun]
        body
]
