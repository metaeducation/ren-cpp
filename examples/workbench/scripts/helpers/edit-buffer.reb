Rebol [
    Title: {Ren Garden Edit Buffer Helpers}

    Description: {
        The user may ask to preload the buffer, the dialect (at present) 
        allows them to make selections as well.  So it might look like:
    
            console/meta ["Hello" space || "Selection" | space "World"] 
    
        The | indicates the position point of a selection, and the || 
        indicates the anchor.  What Ren Garden needs back is a triple of 
        the total string and the integer positions of the start and the end.
        So the above would need to produce:

            ["Hello Selection World" 15 6]
     
        (Bear in mind that if a string is N characters long, it has N + 1
        positions... and we're using Rebol indexing so they can start at 
        1 and go up to N in the return result.  If the position and 
        anchor are equal, then the selection is "collapsed" to a point.)

        This was an early demo of a piece of functionality that was easier
        to write as a Rebol script than as the corresponding C++ code.
        So it was broken out into a "Helper".  The ultimate balance of
        power may be shifted such that more of the functionality of Ren
        Garden is driven by the scripts, so the shape of these interfaces
        may change to be more of a "don't call me, I'll call you" with
        the selection requests coming from the scripting side.
    }

    License: 'BSD
]


console-buffer-helper: function [value [block! string!]] [

    ; A plain old string with no markers on it is just treated
    ; like a collapsed selection at the very tail of the string
    ; (position = anchor)

    if string? value [
        return reduce [value (1 + length value) (1 + length value)]
    ]


    ; Otherwise it's a block, so see if there are any markers
    ; inside of it.

    ; !!! What if the markers aren't at the top level?  combine does permit
    ; nested block structure, so this should perhaps be done with some kind
    ; of FIND/DEEP in a perfect world (that would make all this tricky!)

    position: find value '|
    anchor: find value '||


    ; Give an error back if they've used too many anchors.  (Might it be
    ; okay if they use | twice, though, to indicate they don't care
    ; which end of the selection the cursor winds up on?  It's easy to
    ; forget and do that...)
 
    if any [
        find next position '|
        find next anchor '||
    ] [
        do make error! {
            Buffer dialect only permits one | and || mark in a block
        }
    ]


    ; Three cases to cover: no markers, just |, and both | and ||

    case [

        ; simple case: a block with no selection markers is run
        ; through combine, with cursor at the end of the generated string
        ; For a description of combine and how it works, see:
        ;
        ;    http://blog.hostilefork.com/combine-alternative-rebol-red-rejoin/
        not any [position anchor] [
            buffer: combine value
            position-index: anchor-index: 1 + length buffer
        ]


        ; If there's only one mark, then we do the combine in two phases:
        ; the part before it, and the part after it.  We use the length
        ; of the first half of the combination to find the cursor position

        all [position (not anchor)] [
            buffer: combine/part value position
            position-index: anchor-index: 1 + length buffer

            combine/into (next position) (tail buffer)
        ]


        ; Two marks...a similar method to before, just with three divisions
        ; for the combine instead of two

        true [
            ; The only difference between the marks being | then || vs
            ; || then | is which side the cursor winds up on.  A forward
            ; selection will have the anchor before the position.  If
            ; it's not a forward selection, swap the positions and then
            ; swap the results later.

            backward: (index-of position) < (index-of anchor)
            if backward [
                ; @HostileFork had the idea for a SWAP-VALUES and notes
                ; that @Maxim came up with the same name for the same
                ; function implementation, so perhaps it should exist:
                ;
                ; http://www.rebol.org/aga-display-posts.r?post=r3wp824x957

                temp: position
                position: anchor
                anchor: temp
            ]

            buffer: combine/part value anchor
            anchor-index: 1 + length buffer

            combine/part/into (next anchor) position (tail buffer)
            position-index: 1 + length buffer

            combine/into (next position) (tail buffer)

            ; If necessary, reverse the index results to account for our
            ; earlier reversal of marker positions

            if backward [
                temp: anchor-index
                anchor-index: position-index
                position-index: temp
            ]
        ]
    ]


    ; Return the triple as the final result from the dialect

    return reduce [buffer position-index anchor-index]
]
