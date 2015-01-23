Rebol [
    Title {Removal equivalences of CONTEXT and OBJECT, challenge MODULE}

    Description {
        The words CONTEXT and OBJECT were used somewhat interchangeably,
        which is confusing.  The better word for what the Rebol concept is
        would be CONTEXT.  A more OOP abstraction with constructors,
        destructors and such could reuse OBJECT.

        This breaks too many things ATM to actually use in the system. But
        contains "talking points" about the terminology and questions about
        whether so many words are needed.
    }
]

;context?: :object?
;object?: does [make error! "use CONTEXT? instead of OBJECT?"]


;has: :context


;object: does [make error! "use HAS or CONTEXT instead of OBJECT"]


;context: func [
;    spec [block!] {Meta information configuring the context}
;    body [block!] {Contents of the context}
;] [
;    ; study module system design to borrow features for context
;    has body
;]


;module: does [make error! "Use CONTEXT instead of MODULE"]


wrap: func [
    "Evaluates a block, wrapping all set-words as locals."
    body [block!] "Block to evaluate"
] [
    do bind/copy/set body make object! 0
]
