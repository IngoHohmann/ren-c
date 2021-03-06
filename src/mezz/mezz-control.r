REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "REBOL 3 Mezzanine: Control"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
]

launch: func [
    {Runs a script as a separate process; return immediately.}
    script [file! string! blank!] "The name of the script"
    /args arg [string! block! blank!] "Arguments to the script"
    /wait "Wait for the process to terminate"
][
    if file? script [script: file-to-local clean-path script]
    args: reduce [file-to-local system/options/boot script]
    unless void? :arg [append args arg]
    either wait [call/wait args] [call args]
]

wrap: func [
    "Evaluates a block, wrapping all set-words as locals."
    body [block!] "Block to evaluate"
][
    do bind/copy/set body make object! 0
]
