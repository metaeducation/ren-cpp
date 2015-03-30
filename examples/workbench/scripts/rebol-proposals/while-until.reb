Rebol [
    Title {Reimagination of until and while}

    Description {
        http://curecode.org/rebol3/ticket.rsp?id=2163
    }
]

;
; NOTE: These are currently broken due to the issue here:
;
;     http://issue.cc/r3/539
;

while: func [
    {While a condition block is TRUE?, evaluates another block.}
    cond-block [block!]
    body-block [block!]
    /after {Run the body block once before checking the condition}
    /local result
] [
    if after [result: do body-block]
    system/contexts/lib/while cond-block [
        set/any 'result do body-block
        ]
    get/any 'result
]

until: func [
    {Until a condition block is TRUE?, evaluates another block.}
    cond-block [block!]
    body-block [block!]
    /after {Run the body block once before checking the condition}
    /local result
] [
    if after [set/any 'result do body-block]
    system/contexts/lib/while [not do cond-block] [
        set/any 'result do body-block
    ]
    get/any 'result
]

; REPEAT is the usual word-companion of UNTIL, but if UNTIL is to be lined
; up as the parallel to WHILE it becomes
;
;     x: 0
;     repeat [
;         + xx
;         x > 9
;     ]
;
; And then returning 10 from that.  Basically, what UNTIL used to do...
; REPEAT this code block until you get something that is TRUE?
;
; @earl believes there has been some natural research into the idea that
; REPEAT VAR [...DIALECT...] [...CODE...] may be the best name for the
; "looping dialect".  But I think LOOP is better and has more precedent
; in relevant languages.

repeat: :system/contexts/lib/until
