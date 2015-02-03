Rebol [
    Title: {LOOP Dialect}

    Description: {
        This is a beginning on the missing (and often requested) range-based
        "Loop Dialect".  It allows one to write things like:

            >> loop x [1 thru 3] [print x]
            1
            2
            3

        Though it is a large design space for a dialect, that seems like a
        basic good start.  Using the words TO and THRU consistent with
        PARSE is a starting example of how such a dialect could seek to
        present a coherent mindset that would help Rebol/Red programmers
        use their early knowledge to apply later.

        (This contrasts with other proposals that have tried to mimic other
        languages with [1 .. 3] or similar; and it's possible to support
        both if it ultimately turns out to be necessary.)

        Note that it's not possible to write good non-native loop wrappers due
        to a blocking and high-priority bug ATM:

            http://issue.cc/r3/539

        Which is a fancy way of saying "these are all broken".  The moment
        you try to return out of the body of a loop, it will just terminate
        the loop and not return from the calling context.

        @earl says he'll be fixing it.  :-)

            http://chat.stackoverflow.com/transcript/message/21300414#21300414

        In the meantime, these can be experimented with.  They should be
        rewritten as natives either way.

        A non-destructive /REVERSE is a very important option and needs to be
        added to any iterator that can support it...although the range dialect
        will not be able to do so efficiently if it depends on an imperative
        generator (unless the generator can be run in reverse?)
    }
]



; This dialect has not been designed yet, so we just assume it's [X to Y]
; or [X thru Y].  A /REVERSE option is very desirable but may be
; technically difficult to implement in the general case.  It may offer
; it but throw an error on special "irreversible" generators.

make-looper: function [
    {Generate a function from a LOOP dialect block, yielding values until NONE}
    spec [block!]
    /reverse
] [
    start: reduce first spec
    finish: reduce last spec
    if 'to = second spec [
        -- finish
    ]
    if reverse [
        temp: start
        start: finish
        finish: temp
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



; @GreggIrwin suggested that he thought LOOP was the best foundational name
; for a "looping construct" (vs. FOR).  After thinking about it, @HostileFork
; came to agree.  Consider:
;
;     for x [1 to 10] [print x]
;
; That doesn't have a very clear name for those not-steeped in programing
; history.  The not-used-very-often LOOP construct has a name that's nearly
; as short and doesn't carry that baggage:
;
;     loop x [1 to 10] [print x]
;
; Calling it the "Loop Dialect" vs. the "For Dialect" also makes more sense.
; So stealing the current "loop" from its barely-used current "do this body
; exactly N times" is a worthwhile cost.

loop: func [
    {Set a variable and run a body of code for every value in a LOOP dialect}
    'word
    spec [block!]
    body [block!]
    /local fun
] [
    looper: make-looper spec
    while [not none? set/any word looper]
        body
]



; A version of loop where you don't care about naming a variable and just
; want to do something for each time something is returned is hard to name.
; REPEAT is available but I don't know that REPEAT has any shade of
; meaning distinct from LOOP to suggest no variable.  It seems better to
; just say:
;
;    loop none [1 to 10] [...stuff...]
;
; But due to quoting, that would be hard-coding a word name that couldn't be
; used.  (What if you *wanted* to set a variable named "none" each time
; through the loop?)  An alternative would be to name the construct LOOP-NONE:
;
;    loop-none [1 to 10] [...stuff...]
;
; That has the nice property of *learnability*, while REPEAT vs LOOP carry
; no good distinction.  On the downside, people might forget the hyphen and
; be confused at what's happening to none in the body of the loop...at
; which point one is tempted to add a check for that specific word...at
; which point you probably should have just made a special disallowance for
; using none as a loop variable.  :-/
;
; So it seems one is stuck with "repeat" and saying "You repeat this body
; a certain number of times".  Saying it accepts a "Loop Dialect" block
; would be deferent to LOOP; it also has the longer name so you'd imagine
; it's the one used less often.

repeat: func [
    {Run a body of code for each value in a LOOP dialect, without a variable}
    spec [block!]
    body [block!]
    /local fun
] [
    looper: make-looper spec
    while [not none? looper]
        body
]



; With the decision to name the "Loop Dialect" vs. calling it the "For Dialect"
; the name FOR became free for its traditional construct.  Which is good
; because C-FOR is a doubly awkward name.  This should probably stay
; implemented as a Mezzanine despite the inefficiency; because supporting
; such a construction is more of a "showcase of what's possible for people
; holding onto the past" than it is a construct which is terribly
; interesting for usage in long-term Rebol programming.
;
; http://chat.stackoverflow.com/transcript/message/15915182#15915182
; http://issue.cc/r3/884

for: func [
    {Execute a "C-style" FOR loop with an initialization, test, and step}
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



; "FOREACH" is bad as a name; it's hard to scan, and given that it's a
; non-word the eye often has the word "REACH" jump out of it.  EACH is
; shorter, known well by its name in systems like jQuery, and given the
; distancing from "FOR" makes more sense to be using.

each: :lib/foreach
foreach: does [do make error! "foreach is now EACH"]



; The idea of visiting every position in a series vs. every value is an
; odd one, which is what REPEAT does.  The distinction between "With EACH
; element do this body" and "With EVERY position in the series do this
; body" is questionable, but it's a tough one to name.  Giving the shorter
; and more common programming name to the more frequently used primitive
; is at least a start.

every: :lib/repeat
repeat: does [do make error! "repeat is now EVERY"]



; "FORALL" is a hard one to name, and actually a difficult one to get
; one's head around in the first place.  It combined the weirdness of
; REPEAT with the idea of modifying the series variable itself in the
; body of the loop.
;
;     >> x: [1 2 3 4] forall x [probe x]
;     [1 2 3 4]
;     [2 3 4]
;     [3 4]
;     [4]
;
; That raises mysterious questions about what the value would be at the end
; of the iteration...or if that would be different if you broke out of the
; loop?  The puzzling behaviors have been a source of confusion and apparently
; changed between versions due to problems.
;
; But it is useful, mostly because it lets you modify the series as you
; go and then touch up the series position, letting the construct
; continue to move you.  This is similar to iterators in other languages.
; In a sense you are using the series itself as its own iterator.
; Used as a transitive verb, you can see it taking the series and use
; it as its own iterator.  So ITERATE is a good starting name for it.
;
; Generally speaking it would be nicer if there were some clearer pattern to
; the relationship between EVERY and ITERATE.  Also especially nice would
; be a /REVERSE on this one.

iterate: :lib/forall
forall: does [do make error! "forall is now ITERATE"]
