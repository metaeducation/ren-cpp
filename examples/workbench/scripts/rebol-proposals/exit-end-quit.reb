Rebol [
    Title: {Exit, End, Quit proposal}

    Description {
        TBD
    }
]

quit: func [
    {Stops evaluation and exits the interpreter with a status code of 0}
] [
    system/contexts/lib/quit
]

exit: func [
    {Stops evaluation and exits the interpreter, returning a status code.}

    status [integer!] {Varies by platform, see http://en.wikipedia.org/wiki/Exit_status}
] [
    system/contexts/lib/quit/return status
]

;-- Can't be a wrapper function because it would return the exit
end: :system/contexts/lib/exit


;-- Rename /QUIT to /EXIT to signify that you're getting the code

catch: func [
    {Catches a throw from a block and returns its value.}
    block [block!] "Block to evaluate"
    /name "Catches a named throw"
    word [word! block!] "One or more names"
    /exit "Special catch for EXIT and QUIT natives, returns exit status"
] [
    apply :system/contexts/lib/catch [block name word exit]
]
