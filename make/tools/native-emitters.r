REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Generate auto headers"
    File: %natives-emitters.r
    Rights: {
        Copyright 2017 Atronix Engineering
        Copyright 2017 Rebol Open Source Contributors
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Needs: 2.100.100
]

emit-native-proto: proc [
    "Emit native prototypes to @unsorted-buffer"
    proto
][
    if all [
        block? proto-parser/data
        any [
            'native = proto-parser/data/2
            all [
                path? proto-parser/data/2
                'native = proto-parser/data/2/1
            ]
        ]
    ] [
        line: text-line-of proto-parser/parse.position

        if not block? proto-parser/data/3 [
            fail [
                "Native" (uppercase form to word! proto-parser/data/1)
                "needs loadable specification block."
                (mold the-file) (line)
            ]
        ]

        append case [
            ; could do tests here to create special buffer categories to
            ; put certain natives first or last, etc. (not currently needed)
            ;
            true [ ;-- R3-Alpha needs to bootstrap, do not convert to an ELSE!
                unsorted-buffer
            ]
        ] unspaced [
            newline newline
            {; !!! DO NOT EDIT HERE! This is generated from }
            mold the-file { line } line newline
            mold/only proto-parser/data
        ]

        proto-count: proto-count + 1
    ]
]

emit-include-params-macro: procedure [
    "Emit macros for a native's parameters"

    e [object!] "where to emit (see %common-emitters.r)"
    word [word!] "name of the native"
    paramlist [block!] "paramlist of the native"
][
    ; start emitting what will be a multi line macro (backslash as last
    ; character on line is how macros span multiple lines in C).
    ;
    e/emit-line [
        {#define} space "INCLUDE_PARAMS_OF_" (uppercase to-c-name word)
        space "\"
    ]

    ; Collect the argument and refinements, converted to their "C names"
    ; (so dashes become underscores, * becomes _P, etc.)
    ;
    n: 1
    for-each item paramlist [
        if all [any-word? item | not set-word? item] [
            param-name: to-c-name to-word item

            e/emit-line/indent [
                either refinement? item ["REFINE"] ["PARAM"]
                    "(" n "," space param-name ");" space "\"
            ]
            n: n + 1
        ]
    ]

    comment [
        ; Get rid of trailing \ for multi-line macro continuation.
        e/unemit newline
        e/unemit #"\"
        e/emit newline
    ]

    e/emit-line [spaced-tab "Enter_Native(frame_);"]
]
