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
index?: does [do make error! "index? is now index-of"]

offset-of: :system/contexts/lib/offset?
offset?: does [do make error! "offset? is now offset-of"]


; Use type-of to get the type of something.
;
; With "type?" freed up, we could actually use type! as the type name
; for, well, types. And then of course use "type?" as type-predicate for
; type!.

type-of: :system/contexts/lib/type?
type?: does [do make error! "type? is now type-of"]


;-- If "bind-of" always returned a context, shouldn't it be context-of?

context-of: :system/contexts/lib/bind?
bind?: does [do make error! "bind? is now context-of"]
bound?: does [do make error! "bound? is now context-of"]


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
