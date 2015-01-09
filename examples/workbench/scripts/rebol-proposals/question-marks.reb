Rebol [
    Title: {Question mark revisted}

    Description: {
        End in question marks only if a boolean result.  Adjust to drop 
        the question mark if not, or add an -of, or other rethinking.
    }
]

;-- length is so common that it deserves the word (think head, tail...)
;-- indexing not so common

length: :system/contexts/lib/length?
unset 'length?

index-of: :system/contexts/lib/index?
unset 'index?

offset-of: :system/contexts/lib/offset?
unset 'offset?


; Use type-of to get the type of something
; type? could actually be shorter for datatype?

type-of: :system/contexts/lib/type?
type?: :system/contexts/lib/datatype?
unset 'datatype?


;-- If "bind-of" always returned a context, shouldn't it be context-of?

context-of: :system/contexts/lib/bind?
unset 'bind?


; would get media codec name of a file, how about codec-of

codec-of: :system/contexts/lib/encoding?
unset 'encoding?


; sign-of is more sensible than sign?

sign-of: :system/contexts/lib/sign?
unset 'sign?


;-- more to look over

;file-type?
;speed?
;suffix?
;why?
;info?
;exists?
