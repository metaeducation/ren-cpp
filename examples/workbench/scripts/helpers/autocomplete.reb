Rebol [
    Title: {Ren Garden Autocomplete Helper}

    Description: {
        Autocomplete logic is based on the taking a single (possibly
        partial) piece of the text for a Ren token--along with a 
        bit of selection state...and deciding how to complete it
        within a context.  So if | represents the cursor position
        and || a possible starting point, we might start like this:

            app| 

        A completion might then move the editor to a state like:

            app||end|

        Another completion might go forward to:

            app||ly|

        A request to go backwards should be able to return to:

            app||end|

        This simple idea of completion is dependent solely on selection;
        so there is *no hidden state*.  Making it even simpler, you only get
        a single point of selection with the idea that all completions are
        implicitly to the end.  The api hands in a priority-ranked list of
        contexts to be searched for lookup, the text string, and a cursor
        position within that string...so the above is:

            [ctx1 ctx2 ...] {app} 4

            [ctx1 ctx2 ...] {append} 4

            /backward [ctx1 ctx2 ...] {apply} 4

        (Note: There are 4 selection positions when dealing with a 3
        character string...the first position is *before* the first
        character!)
           
        Currently each completion call rediscovers the state...which 
        is hopefully deterministic.  This means that the WORDS-OF for the
        contexts should give the same answers in the same order each time
        if nothing is added or removed.

        Presently, Ren Garden leaves it open ended to be given something like:

            apqp|blahblah

        ...and come back with a guess like:

            app|end

        So it lets you move the completion index if you wanted in the return
        result, as well as rewrite the string completely.  Neither is used
        here yet in this completer.  (We "autocomplete", but we do not
        "autocorrect".)

        While by far not the fanciest interface for autocomplete one might
        imagine, it does offer the ability to distribute the work more easily
        between Ren Garden and a Rebol script.  Consider it a starting point
        for more complex behavior.
    }

    License: 'BSD
]


;
; We want to be able to complete things that are "pathy" in the simple case
; that the path is a chain of one or more objects.  But we don't want to
; use the evaluator and run the risk of side-effects or infinite loops!
; Because if someone wrote:
;
;    foo/baz/b|
;
; ...we want to know if foo/baz is a context we can pick from, without running
; arbitrary code.  So we "interpret" the path looking for a chain of objects
; and see if we find something we can "complete" in.  (Currently that means
; fields in objects and refinements for functions.)
;
; The thing we find--be it a function or an object--is shown in the explorer
; as the thing you're given "help" about.
;

try-get-scope-from-path: function [path [path!]] [
    obj: _

    for-each element path [

        ; only consider paths made up of words...

        unless word? :element [return _]


        ; If we have an object in progress, look up the word in it,
        ; otherwise get the word.

        value: either obj [
            unless in obj element [return _]
            select obj element
        ][
            get/any element
        ]

        if any-function? :value [
            return :value
        ]


        ; Completing other types besides object requires mind-reading, and
        ; will be in a future version, probably starting with integer
        ;
        ;    10| (tab)
        ;
        ;    1020
        ;
        ; How did it know?

        unless object? :value [return _]

        obj: value
    ]

    return obj
]



; We can complete in a function by letting you autocomplete
; refinements...so we make a fake context.  Note that once we see a
; function we stop checking, so you could say:
;
;     append/banana/on|
;
; ...and get it to complete with only, despite no /banana refinement

fake-context-from-function: function [fun [any-function!]] [
    spec: copy []
    for-each word words-of :fun [
        if refinement? word [
            append spec to-set-word word
        ]
    ]
    append spec _
    return has spec
]



;
; The exported hook called by Ren Garden; needs to return three values in
; a block:
;
;    - completed text string
;    - index position for start of completion
;    - value to browse in the explorer
;

autocomplete-helper: function [
    contexts [block!] 
        {List of contexts (object!) to be searched, in priority order}

    text [string!] 
        {The text input so far}

    index [integer!] 
        {Cursor position (or anchor position) in text, indicates "stem"}

    /backward
        {If we should search backward in the enumeration}
][
    ; If you are at the beginning of a word and hit "tab", what are you
    ; asking to have completed?  :-/  Leave it alone for now.

    if index = 1 [
        return reduce [text index _]
    ]


    ; What we do here is see if the thing we're trying to complete looks
    ; "pathy", and if we're completing it somewhere after the last slash.
    ; If so we try some interpretation on the path to get object fields
    ; or function refinements to complete.

    last-slash: find/last text "/"
    if all [
        last-slash
        index > index-of last-slash
    ][
        fragment: next last-slash
        stem: copy/part (next last-slash) (index - index-of next last-slash)
        base: copy/part text last-slash

        success: false
        try [
            loaded: load/type base 'unbound
            success: true
        ]

        ; Now that we've loaded the path, put the trailing slash back on the
        ; autocomplete base.  (If we autocompleted to actual ANY-VALUE!, this
        ; stuff won't be necessary...)
        ;
        append base "/"

        if all [success any [path? loaded word? loaded]] [
            path: to-path loaded

            ; We loaded the path (or the word that would have been the head
            ; of a path, had we not swiped out the slash).  Yet we loaded
            ; unbound so that we could bind it in the priority ranked contexts
            ; we were given--we shouldn't assume!
            ;
            ; Go from lowest priority to highest priority and try binding,
            ; potentially overriding each time.  This would ideally use
            ; for-each/reverse, but that can't be implemented as a mezzanine
            ; yet due to a bug.

            iter: back tail contexts
            final: false
            while [not any [final (final: head? iter false)]] [
                path: first bind reduce [path] iter/1
                iter: back iter
            ]

            ; With the path now (maybe) bound, we can interpret it and look
            ; to see if we have an item we might call a "scope" for it
            ; (either a function or an object)

            scope: try-get-scope-from-path path

            if function? :scope [
                completion-contexts: reduce [fake-context-from-function :scope]
            ]

            if object? :scope [
                completion-contexts: reduce [:scope]
            ]
        ]
    ]

    ; If our path-trickery above didn't get us any completion-contexts, then
    ; just do a "global" search with the ones that were passed in.  Note that
    ; Ren Garden typically passes us two right now...the context for the tab
    ; window you are in, followed by LIB.  There seems to be a strong need
    ; for a notion of inheritance in contexts that is more general.  :-/
    ;
    unless set? 'completion-contexts [
        completion-contexts: copy contexts
        fragment: text
        stem: copy/part text (index - 1) ;-- index is a *cursor* position
        base: copy ""
    ]

    ; If we are doing a backward completion via shift-tab, we need to
    ; enumerate backward.  But we still need to keep the completion-contexts
    ; intact, because even if our enumeration is backwards the priority
    ; of which context "wins" in lookup is still forwards!
    ;
    adjusted-contexts: either backward [
        reverse copy completion-contexts
    ][
        completion-contexts
    ]

    ; We track the first candidate we have seen for completion, but do not
    ; take it immediately.  That's because we might find an word corresponding
    ; to the current complete "state" embodied by the selection, and want
    ; whichever one is after that instead.

    first-candidate-word: _
    first-candidate-ctx: _
    take-next: false

    for-each ctx adjusted-contexts [

        words: words-of ctx
        if backward [reverse words]
        
        for-each word words [
            spelling: spelling-of word

            ; Not a very sophisticated search ATM, we just walk through
            ; matching all words with the same beginning stem.
            ;
            ; Is there a more efficient COMPARE/PART technique vs. copying?

            if stem != copy/part spelling (length stem) [
                continue
            ]

            ; When evaluating a match candidate, we first must consider if
            ; we should actually be looking at this candidate at all.  We
            ; should not be if it is "outranked" by an entry in a higher
            ; priority context.  So if you have two contexts:
            ;
            ;    higher: [
            ;       aa: ...
            ;       ab: ...
            ;       ac: ...
            ;    ]
            ;
            ;    lower: [
            ;       ab: ...
            ;       ad: ...
            ;    ]
            ;
            ; We will visit the lower context's AD, but not its AB as a
            ; candidate.  Otherwise the completion would loop:
            ;
            ;     aa => ab => ac => ab => ac => ab => ...

            outranked: false
            for-each prior-ctx completion-contexts [
                if prior-ctx = ctx [
                    break ;-- no more prior contexts
                ]

                if in prior-ctx word [
                    outranked: true
                    break
                ]
            ]

            if outranked [
                continue
            ]

            ; If we see an exact match of the spelling, then it is effectively
            ; thought of as a "state of prior completion".  Yet it may not
            ; have been supplied by a previous autocomplete; the user might
            ; just have typed it in!  But we handle it the same either way,
            ; by interpreting it as an instruction to take the next
            ; candidate that we see.
            ;
            either spelling = fragment [
                assert [not take-next]
                take-next: true
            ][
                ; As long as this word wasn't outranked, we can use it if we
                ; were told to take the next one we see.

                if take-next [
                    return reduce [
                        unspaced [base spelling]
                        index
                        any [:scope | in ctx word]
                    ]
                ]
            ]

            ; This still might be a candidate for completion, if we reach
            ; the end of the loops.
            ;
            unless first-candidate-word [
                first-candidate-word: word
                first-candidate-ctx: ctx
            ]
        ]
    ]

    ; If we get to the end, then we just take the first candidate for
    ; completion if there was one (whether take-next was set or not)
    ;
    if first-candidate-word [
        return reduce [
            unspaced [base _ spelling-of first-candidate-word]
            index
            any [:scope (in first-candidate-ctx first-candidate-word)]
        ]
    ]

    ; Didn't find anything, just return what we were given...
    ;
    return reduce [text index _]
]
