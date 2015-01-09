Rebol [
   Title: {Reform of the TO operation}     
]

;
; Let's start with a more coherent objective for to-string as being
; whatever FORM was trying to be, and work from there
;

to: func [
    "Converts to a specified datatype."
    type [any-type!] "The datatype or example value"
    value [any-type!] "The value to convert"
] [
    switch/default to-word type [
       string! [system/contexts/lib/form value]
    ] [
       system/contexts/lib/to type value
    ]
]

to-string: func [
    "Converts to string! value." 
    value
] [
    to string! :value
]

unset 'form

spelling-of: func [
    "Gives the delimiter-less spelling of words or strings"
    value [any-word! any-string!]
] [
    switch/default system/contexts/lib/type?/word value [
        string! [value]
        email! [value] ;-- what about the @someone issue?
        url! [value]
        tag! file! [system/contexts/lib/to string! value]
        word! [system/contexts/lib/to string! value]
        refinement! lit-word! get-word! issue [remove system/contexts/lib/to string! value]
        set-word! [head remove back tail system/contexts/lib/to string! value]
    ] [
        do make error! "Unhandled case in spelling-of"
    ]
]
