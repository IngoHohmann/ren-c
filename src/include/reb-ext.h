//
//  File: %reb-ext.h
//  Summary: "R3-Alpha Extension Mechanism API"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2017 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//=////////////////////////////////////////////////////////////////////////=//
//
// These are definitions that need to be visible to both %a-lib.c and
// "libRebol" clients.
//
// Historically, routines exported as libRebol were prefixed by "RL_"
// (Rebol Lib).  Interactions with the garbage collector were quite shaky,
// because they used their own proxy for REBVAL cells which contained raw
// pointers to series...and generally speaking, raw series pointers were
// being held in arbitrary locations in user code the GC could not find.
//
// Ren-C split this into two kinds of clients: one that can use the internal
// API, including things like PUSH_GUARD_VALUE() and SER_HEAD(), with all
// the powers and responsibility of a native in the EXE.  Then the libRebol
// clients do not know what a REBSER is, they only have REBVAL pointers...
// which are opaque, and they can't pick them apart.  This means the GC
// stays in control.
//
// Clients would use the libRebol API for simple embedding where the concerns
// are mostly easy bridging to run some Rebol code and get information back.
// The internal API is used for extensions or the authoring of "user natives"
// which are Rebol functions whose body is a compiled string of C code.
//

#include "reb-defs.h"

// This table of types used to be automatically generated by complex scripts.
// Yet the original theory of these values is that they would be kept in a
// strict order while REB_XXX values might be rearranged for other reasons.
// While the future of the RL_API is in flux, these are now just hardcoded
// as an enum for simplicity, and the tables mapping them to Rebol types are
// built by C code in RL_Init().
//
// !!! It was purposefully the case in R3-Alpha that not all internal REB_XXX
// types had corresponding RXT_XXX types.  But its not clear that all such
// cases were excluded becaues they weren't supposed to be exported...some
// may have just not been implemented.  Now that "RXIARG" is not a separate
// entity from a REBVAL, "exporting" types should be less involved. 
//
// !!! Currently these are hardcoded at their "historical" values, which
// gives a feeling of how it might come to have gaps over time if this
// parallel table which tries to stay constant is kept.  Though there's no
// code that could successfully link against the other changes to the API,
// so they could be compacted if need be.
//

enum REBOL_Ext_Types
{
    RXT_0 = 0, // "void" indicator, though not technically a "datatype"

    RXT_BLANK = 1,
    RXT_HANDLE = 2,
    RXT_LOGIC = 3,
    RXT_INTEGER = 4,
    RXT_DECIMAL = 5,
    RXT_PERCENT = 6,
    
    RXT_CHAR = 10,
    RXT_PAIR = 11,
    RXT_TUPLE = 12,
    RXT_TIME = 13,
    RXT_DATE = 14,

    RXT_WORD = 16,
    RXT_SET_WORD = 17,
    RXT_GET_WORD = 18,
    RXT_LIT_WORD = 19,
    RXT_REFINEMENT = 20,
    RXT_ISSUE = 21,

    RXT_STRING = 24,
    RXT_FILE = 25,
    RXT_EMAIL = 26,
    RXT_URL = 27,
    RXT_TAG = 28,

    RXT_BLOCK = 32,
    RXT_GROUP = 33,
    RXT_PATH = 34,
    RXT_SET_PATH = 35,
    RXT_GET_PATH = 36,
    RXT_LIT_PATH = 37,

    RXT_BINARY = 40,
    RXT_BITSET = 41,
    RXT_VECTOR = 42,
    RXT_IMAGE = 43,

    RXT_GOB = 47,
    RXT_OBJECT = 48,
    RXT_MODULE = 49,

    RXT_MAX
};

typedef unsigned char REBRXT;
