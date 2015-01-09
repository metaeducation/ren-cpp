Rebol [
    Title {Elimination of shell commands from language core}

    Description {
        Rebol has often been used as a kind of console but it is poor at 
        that, and several "shorthands" (as far as T for running test.reb)
        have managed to make it into the language core.  For a general
        purpose language focusing on a literate specification, words like
        LS or CD (which are shorthands or CHANGE-DIR and LIST-DIR) are 
        not appropriate. to include.  Q also causes a mysterious problem
        if you type APPEND [X Y Z] Q (or similar) and it seems the 
        interpreter crashed with no message.

        For those of you who think these are so precious, there's a
        better proposal on the table...
    }
]

unset 'ls
unset 'cd
unset 'pwd

unset 'q

unset 't

unset 'dt
unset 'dp
