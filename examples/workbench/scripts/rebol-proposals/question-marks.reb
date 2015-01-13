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
length?: does [do make error! "length? is now length"]

index-of: :system/contexts/lib/index?
unset 'index?

offset-of: :system/contexts/lib/offset?
unset 'offset?


; Use type-of to get the type of something
; type? could actually be shorter for datatype?

type-of: :system/contexts/lib/type?
type?: :system/contexts/lib/datatype?
datatype?: does [do make error! "datatype? is now type?"]


;-- If "bind-of" always returned a context, shouldn't it be context-of?

context-of: :system/contexts/lib/bind?
bind?: does [do make error! "bind? is now context-of"]


; would get media codec name of a file, how about codec-of

codec-of: :system/contexts/lib/encoding?
encoding?: does [do make error! "encoding? is now codec-of"]


; sign-of is more sensible than sign?

sign-of: :system/contexts/lib/sign?
sign?: does [do make error! "sign? is now sign-of"]


;-- more to look over

;file-type?
;speed?
;suffix?
;why?
;info?
;exists?
