Rebol [
    Title {Reimagination of until and while}

    Description {
        http://curecode.org/rebol3/ticket.rsp?id=2163
    }
]

while: func [
    {While a condition block is TRUE?, evaluates another block.}
    cond-block [block!]
    body-block [block!]
    /after {Run the body block once before checking the condition}
    /local result
] [
    if after [result: do body-block]
    system/contexts/lib/while cond-block [
        set/any 'result do body-block]
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
