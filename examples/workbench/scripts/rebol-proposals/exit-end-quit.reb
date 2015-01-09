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

