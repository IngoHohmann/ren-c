REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Infix operator symbol definitions"
    Rights: {
        Copyright 2012 REBOL Technologies
        Copyright 2012-2017 Rebol Open Source Contributors
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0.
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Purpose: {
        In R3-Alpha, an "OP!" function would gather its left argument greedily
        without waiting for further evaluation, and its right argument would
        stop processing if it hit another "OP!".  This meant that a sequence
        of all infix ops would appear to process left-to-right, e.g.
        `1 + 2 * 3` would be 9.

        Ren-C does not have an "OP!" function type, it just has ACTION!, but
        a WORD! can be SET with the /ENFIX refinement.  This indicates that
        when the function is dispatched through that word, it should get its
        first parameter from the left.  However it will obey the parameter
        conventions of the original function (including quoting).  Hence since
        ADD has normal parameter conventions, `+: enfix :add` would wind up
        with `1 + 2 * 3` as 7.

        So a new parameter convention indicated by ISSUE! is provided to get
        the "#tight" behavior of OP! arguments in R3-Alpha.
    }
]

; R3-Alpha has several forms illegal for SET-WORD! (e.g. `<:`)  Ren-C allows
; more of these things, but if they were top-level SET-WORD! in this file then
; R3-Alpha wouldn't be able to read it when used as bootstrap r3-make.  It
; also can't LOAD several WORD! forms that Ren-C can (e.g. `->`)
;
; So %b-init.c manually adds the keys via Add_Lib_Keys_R3Alpha_Cant_Make().
; R3-ALPHA-QUOTE annotates to warn not to try and assign SET-WORD! forms, and
; to bind interned strings.
;
r3-alpha-quote: func [:spelling [word! text!]] [
    either word? spelling [
        spelling
    ][
        bind (to word! spelling) (binding of 'r3-alpha-quote)
    ]
]


; Make top-level words (note: / is added by %b-init.c as an "odd word")
;
+: -: *: _

for-each [math-op function-name] [
    +       add
    -       subtract
    *       multiply
    /       divide ;-- !!! may become pathing operator (which also divides)
][
    ; Ren-C's infix math obeys the "tight" parameter convention of R3-Alpha.
    ; But since the prefix functions themselves have normal parameters, this
    ; would require a wrapping function...adding a level of inefficiency:
    ;
    ;     +: enfix func [#a #b] [add :a :b]
    ;
    ; TIGHTEN optimizes this by making a "re-skinned" version of the function
    ; with tight parameters, without adding extra overhead when called.  This
    ; mechanism will eventually generalized to do any rewriting of convention
    ; one wants (e.g. to switch one parameter from normal to quoted).
    ;
    set/enfix math-op (tighten get function-name)
]


; Make top-level words
;
and+: or+: xor+: _

for-each [set-op function-name] [
    and+    intersect
    or+     union
    xor+    difference
][
    ; The enfixed versions of the set operations currently can't take any
    ; refinements, so we go ahead and specialize them out.  (It's also the
    ; case that TIGHTEN currently changes all normal parameters to #tight,
    ; which creates an awkward looking /SKIP's SIZE.
    ;
    set/enfix set-op (tighten specialize function-name [
        case: false
        skip: false
    ])
]


; Make top-level words for things not added by %b-init.c
;
=: !=: ==: !==: =?: _

; <= looks a lot like a left arrow.  In the interest of "new thought", core
; defines the operation in terms of =<
;
lesser-or-equal?: :equal-or-lesser?

for-each [comparison-op function-name] compose [
    =       equal?
    <>      not-equal?
    <       lesser?
    (r3-alpha-quote "=<") equal-or-lesser?
    >       greater?
    >=      greater-or-equal?

    <=      lesser-or-equal? ;-- !!! https://forum.rebol.info/t/349/11

    !=      not-equal? ;-- !!! http://www.rebol.net/r3blogs/0017.html

    ==      strict-equal? ;-- !!! https://forum.rebol.info/t/349
    !==     strict-not-equal? ;-- !!! bad pairing, most would think !=

    =?      same?
][
    ; !!! See discussion about the future of comparison operators:
    ; https://forum.rebol.info/t/349
    ;
    ; While they were "tight" in R3-Alpha, Ren-C makes them use normal
    ; parameters.  So you can write `if length of block = 10 + 20 [...]` and
    ; other expressive things.  It comes at the cost of making it so that
    ; `if not x = y [...]` is interpreted as `if (not x) = y [...]`, which
    ; all things considered is still pretty natural (and popular in many
    ; languages)...and a small price to pay.  Hence no TIGHTEN call here.
    ;
    set/enfix comparison-op (get function-name)
]


; https://trello.com/c/J7jLQPY5

??: enfix func [
    {If TO-LOGIC of the left is true, return right value, otherwise null}

    return: [<opt> any-value!]
    left [<opt> any-value!]
    right [<opt> any-value!]
][
    if :left [:right] ;-- will voidify result if right is null
]

!!: enfix func [
    {If left isn't null, return it, else return value on the right}

    return: [<opt> any-value!]
    left [<opt> any-value!]
    right [<opt> any-value!]
][
    :left else [:right] ;-- will *not* voidify result if right is null
]

?!: enfix func [ ;-- name suggests shorthand of `left ?? () !! right`
    {If TO LOGIC! of the left is false, return right value, otherwise null}

    return: [<opt> any-value!]
    left [<opt> any-value!]
    right [<opt> any-value!]
][
    if-not :left [:right] ;-- will voidify result if right is null
]


; https://trello.com/c/8NF3DFBM

then: enfix :if

not-then: enfix :if-not


; The -- and ++ operators were deemed too "C-like", so ME was created to allow
; `some-var: me + 1` or `some-var: me / 2` in a generic way.
;
; !!! This depends on a fairly lame hack called EVAL-ENFIX at the moment, but
; that evaluator exposure should be generalized more cleverly.
;
me: enfix func [
    {Update variable using it as the left hand argument to an enfix operator}

    return: [<opt> any-value!]
    :var [set-word! set-path!]
        {Variable to assign (and use as the left hand enfix argument)}
    :rest [any-value! <...>]
        {Code to run with var as left (first element should be enfixed)}
][
    set var eval-enfix (get var) rest
]

my: enfix func [
    {Update variable using it as the first argument to a prefix operator}

    return: [<opt> any-value!]
    :var [set-word! set-path!]
        {Variable to assign (and use as the first prefix argument)}
    :rest [any-value! <...>]
        {Code to run with var as left (first element should be prefix)}
][
    set var eval-enfix/prefix (get var) rest
]


; Lambdas are experimental quick function generators via a symbol.  The
; identity is used to shake up enfix ordering.
;
set/enfix (r3-alpha-quote "=>") :lambda
set (r3-alpha-quote "<-") :identity ;-- not enfix, just affects enfix
set/enfix (r3-alpha-quote "->") :identity


; These constructs used to be enfix to complete their left hand side.  Yet
; that form of completion was only one expression's worth, when they wanted
; to allow longer runs of evaluation.  "Invisible functions" (those which
; `return: []`) permit a more flexible version of the mechanic.

set (r3-alpha-quote "<|") :invisible-eval-all
set (r3-alpha-quote "|>") :right-bar
||: :once-bar
