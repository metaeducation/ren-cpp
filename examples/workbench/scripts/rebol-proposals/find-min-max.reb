Rebol [
    Title: {FIND-MIN and FIND-MAX}

    Description: {
    	MIN and MAX exist as arity 2 for getting back the larger of two values.
    	MINIMUM-OF and MAXIMUM-OF, exist as arity 1 for looking for the largest
    	item in a series, but somewhat confusingly return series positions.
    	This uses proposed replacement names FIND-MIN and FIND-MAX
    }

    Homepage: http://curecode.org/rebol3/ticket.rsp?id=197
]

find-min: :system/contexts/lib/minimum-of
find-max: :system/contexts/lib/maximum-of

unset 'maximum-of
unset 'minimum-of
