//
//  File: %sys-rebval.h
//  Summary: {Definitions for the Rebol Boxed Value Struct (REBVAL)}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2016 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information
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
// REBVAL is the structure/union for all Rebol values. It's designed to be
// four C pointers in size (so 16 bytes on 32-bit platforms and 32 bytes
// on 64-bit platforms).  Operation will be most efficient with those sizes,
// and there are checks on boot to ensure that `sizeof(REBVAL)` is the
// correct value for the platform.  But from a mechanical standpoint, the
// system should be *able* to work even if the size is different.
//
// Of the four 32-or-64-bit slots that each value has, the first is used for
// the value's "Header".  This includes the data type, such as REB_INTEGER,
// REB_BLOCK, REB_STRING, etc.  Then there are 8 flags which are for general
// purposes that could apply equally well to any type of value (including
// whether the value should have a new-line after it when molded out inside
// of a block).  There are 8 bits which are custom to each type--for
// instance whether a key in an object is hidden or not.  Then there are
// 8 bits currently reserved for future use.
//
// The remaining content of the REBVAL struct is the "Payload".  It is the
// size of three (void*) pointers, and is used to hold whatever bits that
// are needed for the value type to represent itself.  Perhaps obviously,
// an arbitrarily long string will not fit into 3*32 bits, or even 3*64 bits!
// You can fit the data for an INTEGER or DECIMAL in that (at least until
// they become arbitrary precision) but it's not enough for a generic BLOCK!
// or a FUNCTION! (for instance).  So those pointers are used to point to
// things, and often they will point to one or more Rebol Series (see
// %sys-series.h for an explanation of REBSER, REBARR, REBCTX, and REBMAP.)
//

//
// Note: Forward declarations are in %reb-defs.h
//

// The definition of the REBVAL struct has a header followed by a payload.
// On 32-bit platforms the header is 32 bits, and on 64-bit platforms it is
// 64-bits.  However, even on 32-bit platforms, some payloads contain 64-bit
// quantities (doubles or 64-bit integers).  By default, the compiler would
// pad a payload with one 64-bit quantity and one 32-bit quantity to 128-bits,
// which would not leave room for the header (if REBVALs are to be 128-bits).
//
// So since R3-Alpha, a `#pragma pack` of 4 is requested for this file:
//
//     http://stackoverflow.com/questions/3318410/
//
// It is restored to the default via `#pragma pack()` at the end of the file.
// (Note that pack(push) and pack(pop) are not supported by older compilers.)
//
// Compilers are free to ignore pragmas (or error on them).  Also, this
// packing subverts the automatic alignment handling of the compiler.  So if
// the manually packed structures do not position 64-bit values on 64-bit
// alignments, there can be problems.  On x86 this is generally just slower
// reads and writes, but on more esoteric platforms (like the C-to-Javascript
// translator "emscripten") some instances do not work at all.
//
// Hence REBVAL payloads that contain quantities that need 64-bit alignment
// put those *after* a platform-pointer sized field, even if that field is
// just padding.  On 32-bit platforms this will pair with the header to make
// enough space to get to a 64-bit alignment
//
// If #pragma pack is disabled, a REBVAL will wind up being 160 bits.  This
// won't give ideal performance, and will trigger an assertion in
// Assert_Basics.  But the code *should* be able to work otherwise if that
// assertion is removed.
//
// Note also that a trick which attempted to put a header into each payload
// won't work.  There would be no way to generically adjust the bits of the
// header without picking a specific instance of the union to do it in, which
// would invalidate the payload for any other type.
//
#pragma pack(4)


//=////////////////////////////////////////////////////////////////////////=//
//
//  VALUE HEADER (uses `struct Reb_Header`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The layout of the header corresponds to the following bitfield
// structure on big endian machines:
//
//    unsigned specific:16;     // flags that are specific to this REBVAL kind
//    unsigned general:8;       // flags that can apply to any kind of REBVAL
//    unsigned kind:6;          // underlying system datatype (64 kinds)
//    unsigned settable:1;      // for debug build only--"formatted" to write
//    unsigned not_end:1;       // not an end marker
//
// Due to a desire to be able to assign all the header bits in one go
// with a native-platform-sized int, this is done with bit masking.
// Using bitfields would bring in questions of how smart the
// optimizer is, as well as the endianness of the underlyling machine.
//
// We use REBUPT (Rebol's pre-C99 compatibility type for uintptr_t,
// which is just uintptr_t from C99 on).  Only the low 32 bits are used
// on 64-bit machines in order to make sure all the features work on
// 32-bit machines...but could be used for some optimization or caching
// purpose to enhance the 64-bit build.  No such uses implemented yet.
//

// `NOT_END_MASK`
//
// If set, it means this is *not* an end marker.  The bit has been picked
// strategically to be in the negative sense, and in the lowest bit position.
// This means that any even-valued unsigned integer REBUPT value can be used
// to implicitly signal an end.
//
// If this bit is 0, it means that *no other header bits are valid*, as it
// may contain arbitrary data used for non-REBVAL purposes.
//
// Note that the value doing double duty as a number for one purpose and an
// END marker as another *must* be another REBUPT.  It cannot be a pointer
// (despite being guaranteed-REBUPT-sized, and despite having a value that
// is 0 when you mod it by 2.  So-called "type-punning" is unsafe with a
// likelihood of invoking "undefined behavior", while it's the compiler's
// responsibility to guarantee that pointers to memory of the same type of
// data be compatibly read-and-written.
//
#define NOT_END_MASK \
    cast(REBUPT, 0x01)

#define GENERAL_VALUE_BIT 8
#define TYPE_SPECIFIC_BIT 16

// `CELL_MASK`
//
// This is for the debug build, to make it safer to use the implementation
// trick of NOT_END_MASK.  It indicates the slot is "REBVAL sized", and can
// be written into--including to be written with SET_END().
//
// It's again a strategic choice--the 2nd lowest bit and in the negative.
// This means any REBUPT value whose % 4 within a container doing
// double-duty as an implicit terminator for the contained values can
// trigger an alert if the values try to overwrite it.
//
// Because this is set on *all* writable value cells, it means that it can
// also be used to distinguish "doubular" REBSER nodes (holders for two
// REBVALs in the same pool as ordinary REBSERs) from an ordinary REBSER
// node, as they will have the cell mask clear.
//
#define CELL_MASK \
    ((REBUPT)0x02) // <-- don't use `cast()`...superfluous here, slows debug

// The type mask comes up a bit and it's a fairly obvious constant, so this
// hardcodes it for obviousness.  High 6 bits of the lowest header byte.
//
#define HEADER_TYPE_MASK \
    ((REBUPT)0xFC)

// In debug builds, there's an additional property checked on cell writes
// where values can be marked as unwritable.  There would be cost to checking
// this in the release build, so it is not intended as a "feature"--just to
// help avoid damaging things like the global BLANK_VALUE.
//
#if !defined(NDEBUG)
    #define VALUE_FLAG_WRITABLE_DEBUG \
        ((REBUPT)0x80000000)
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  REBSER_REBVAL_FLAGs common to both REBSER and REBVAL
//
//=////////////////////////////////////////////////////////////////////////=//
//
// An implementation trick permits the pooled nodes that hold series to hold
// two values.  Since a REBSER is exactly two REBVALs in size, that does not
// leave any room for termination.  But it is implicitly terminated by virtue
// of positioning that node next to another style of node that does *not*
// contain two full values, in order to have just enough spare bits to
// signal a termination.
//
// Because of the overlapped design, there are some flags that have to be
// "stolen" from the REBVAL in order to take care of the garbage collector's
// bookkeeping.  Many other flags live in the REBSER's "info" field (as
// opposed to the shared header).  However, those flags cannot apply to one
// of the "full bandwidth" usages of two REBVALs in the node--only these
// basic overhead flags apply.
//

enum {
    // `REBSER_REBVAL_FLAG_MANAGED` indicates that a series is managed by
    // garbage collection.  If this bit is not set, then during the GC's
    // sweeping phase the simple fact that it hasn't been SER_MARK'd won't
    // be enough to let it be considered for freeing.
    //
    // See MANAGE_SERIES for details on the lifecycle of a series (how it
    // starts out manually managed, and then must either become managed or be
    // freed before the evaluation that created it ends).
    //
    REBSER_REBVAL_FLAG_MANAGED = 1 << (GENERAL_VALUE_BIT + 0),

    // `REBSER_REBVAL_FLAG_MARK` is used by the mark-and-sweep of the garbage
    // collector.  Note that the mark is used for other purposes which need to
    // through and set a generic bit, e.g. to protect against loops in the
    // transitive closure ("if you hit a SER_MARK, then you've already
    // processed this series").
    //
    // Because of the dual purpose, it's important to be sure to not run
    // garbage collection while one of these alternate uses is in effect.
    // It's also important to reset the bit when done, as GC assumes when
    // it starts that all bits are cleared.  (The GC itself clears all
    // the bits by enumerating every series in the series pool during the
    // sweeping phase.)
    //
    REBSER_REBVAL_FLAG_MARK = 1 << (GENERAL_VALUE_BIT + 1),

    // `REBSER_REBVAL_FLAG_ROOT` indicates this should be treated as a
    // root for GC purposes.  It only means anything on a REBVAL if that
    // REBVAL happens to live in the key slot of a paired REBSER--it should
    // not generally be set otherwise.
    //
    REBSER_REBVAL_FLAG_ROOT = 1 << (GENERAL_VALUE_BIT + 2),
};



//=////////////////////////////////////////////////////////////////////////=//
//
//  GENERAL FLAGS common to every REBVAL
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The value option flags are 8 individual bitflags which apply to every
// value of every type.  Due to their scarcity, they are chosen carefully.
//

enum {
    // `VALUE_FLAG_FALSE`
    //
    // Both NONE! and LOGIC!'s false state are FALSE? ("conditionally false").
    // All other types are TRUE?.  To make checking FALSE? and TRUE? faster,
    // this bit is set when creating NONE! or FALSE.  As a result, LOGIC!
    // does not need to store any data in its payload... its data of being
    // true or false is already covered by this header bit.
    //
    VALUE_FLAG_FALSE = 1 << (GENERAL_VALUE_BIT + 3),

    // `VALUE_FLAG_LINE`
    //
    // If the line marker bit is 1, then when the value is molded it will put
    // a newline before the value.  The logic is a bit more subtle than that,
    // because an ANY-PATH! could not be LOADed back if this were allowed.
    // The bit is set initially by what the scanner detects, and then left
    // to the user's control after that.
    //
    // !!! The native `new-line` is used set this, which has a somewhat
    // poor name considering its similarity to `newline` the line feed char.
    //
    VALUE_FLAG_LINE = 1 << (GENERAL_VALUE_BIT + 4),

    // `VALUE_FLAG_THROWN`
    //
    // When a REBVAL slot wishes to signal that it is a "throw" (e.g. a
    // RETURN, BREAK, CONTINUE or generic THROW signal), this bit is set on
    // that cell.
    //
    // The bit being set does not mean the cell contains the thrown quantity
    // (e.g. it would not be the `1020` in `throw 1020`)  That evaluator
    // thread enters a modal "thrown state", and it's the state which hold
    // the value--which must be processed (or converted into an error) before
    // another throw occurs.
    //
    // Instead the bit indicates that the cell contains a value indicating
    // the label, or "name", of the throw.  Having the label quickly available
    // in the slot being bubbled up makes it easy for recipients to decide if
    // they are interested in throws of that type or not.
    //
    // R3-Alpha code would frequently forget to check for thrown values, and
    // wind up acting as if they did not happen.  In addition to enforcing that
    // all thrown values are handled by entering a "thrown state" for the
    // interpreter, all routines that can potentially return thrown values
    // have been adapted to return a boolean and adopt the XXX_Throws()
    // naming convention:
    //
    //     if (XXX_Throws()) {
    //        /* handling code */
    //     }
    //
    VALUE_FLAG_THROWN = 1 << (GENERAL_VALUE_BIT + 5),

    // `VALUE_FLAG_RELATIVE` is used to indicate a value that needs to have
    // a specific context added into it before it can have its bits copied
    // or used for some purposes.  An ANY-WORD! is relative if it refers to
    // a local or argument of a function, and has its bits resident in the
    // deep copy of that function's body.  An ANY-ARRAY! in the deep copy
    // of a function body must be relative also to the same function if
    // it contains any instances of such relative words.
    //
    VALUE_FLAG_RELATIVE = 1 << (GENERAL_VALUE_BIT + 6),

    // `VALUE_FLAG_EVALUATED` is a somewhat dodgy-yet-important concept.
    // This is that some functions wish to be sensitive to whether or not
    // their argument came as a literal in source or as a product of an
    // evaluation.  While all values carry the bit, it is only guaranteed
    // to be meaningful on arguments in function frames...though it is
    // valid on any result at the moment of taking it from Do_Core().
    //
    VALUE_FLAG_EVALUATED = 1 << (GENERAL_VALUE_BIT + 7)
};


//=////////////////////////////////////////////////////////////////////////=//
//
//  TRACK payload (not a value type, only in DEBUG)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// `struct Reb_Track` is the value payload in debug builds for any REBVAL
// whose VAL_TYPE() doesn't need any information beyond the header.  This
// offers a chance to inject some information into the payload to help
// know where the value originated.  It is used by voids (and void trash),
// NONE!, LOGIC!, and BAR!.
//
// In addition to the file and line number where the assignment was made,
// the "tick count" of the DO loop is also saved.  This means that it can
// be possible in a repro case to find out which evaluation step produced
// the value--and at what place in the source.  Repro cases can be set to
// break on that tick count, if it is deterministic.
//

#if !defined(NDEBUG)
    struct Reb_Track {
        const char *filename;
        int line;
    };
#endif

struct Reb_Datatype {
    enum Reb_Kind kind;
    REBARR *spec;
};

// !!! In R3-alpha, the money type was implemented under a type called "deci".
// The payload for a deci was more than 64 bits in size, which meant it had
// to be split across the separated union components in Ren-C.  (The 64-bit
// aligned "payload" and 32-bit aligned "extra" were broken out independently,
// so that setting one union member would not disengage the other.)

struct Reb_Money {
    unsigned m1:32; /* significand, continuation */
    unsigned m2:23; /* significand, highest part */
    unsigned s:1;   /* sign, 0 means nonnegative, 1 means nonpositive */
    int e:8;        /* exponent */
};

typedef struct reb_ymdz {
#ifdef ENDIAN_LITTLE
    REBINT zone:7; // +/-15:00 res: 0:15
    REBCNT day:5;
    REBCNT month:4;
    REBCNT year:16;
#else
    REBCNT year:16;
    REBCNT month:4;
    REBCNT day:5;
    REBINT zone:7; // +/-15:00 res: 0:15
#endif
} REBYMD;

typedef union reb_date {
    REBYMD date;
    REBCNT bits;
} REBDAT;

struct Reb_Time {
    REBI64 nanoseconds;
};

typedef struct Reb_Tuple {
    REBYTE tuple[8];
} REBTUP;


struct Reb_Any_Series {
    //
    // `series` represents the actual physical underlying data, which is
    // essentially a vector of equal-sized items.  The length of the item
    // (the series "width") is kept within the REBSER abstraction.  See the
    // file %sys-series.h for notes.
    //
    REBSER *series;

    // `index` is the 0-based position into the series represented by this
    // ANY-VALUE! (so if it is 0 then that means a Rebol index of 1).
    //
    // It is possible that the index could be to a point beyond the range of
    // the series.  This is intrinsic, because the series can be modified
    // through other values and not update the others referring to it.  Hence
    // VAL_INDEX() must be checked, or the routine called with it must.
    //
    // !!! Review that it doesn't seem like these checks are being done
    // in a systemic way.  VAL_LEN_AT() bounds the length at the index
    // position by the physical length, but VAL_ARRAY_AT() doesn't check.
    //
    REBCNT index;
};

struct Reb_Typeset {
    REBU64 bits; // One bit for each DATATYPE! (use with FLAGIT_64)
};


struct Reb_Any_Word {
    //
    // This is the word's non-canonized spelling.  It is a UTF-8 string.
    //
    REBSTR *spelling;

    // Index of word in context (if word is bound, e.g. `binding` is not NULL)
    //
    // !!! Intended logic is that if the index is positive, then the word
    // is looked for in the context's pooled memory data pointer.  If the
    // index is negative or 0, then it's assumed to be a stack variable,
    // and looked up in the call's `stackvars` data.
    //
    // But now there are no examples of contexts which have both pooled
    // and stack memory, and the general issue of mapping the numbers has
    // not been solved.  However, both pointers are available to a context
    // so it's awaiting some solution for a reasonably-performing way to
    // do the mapping from [1 2 3 4 5 6] to [-3 -2 -1 0 1 2] (or whatever)
    //
    REBINT index;
};


struct Reb_Function {
    //
    // `paramlist` is a Rebol Array whose 1..NUM_PARAMS values are all
    // TYPESET! values, with an embedded symbol (a.k.a. a "param") as well
    // as other bits, including the parameter class (PARAM_CLASS).  This
    // is the list that is processed to produce WORDS-OF, and which is
    // consulted during invocation to fulfill the arguments
    //
    // In addition, its [0]th element contains a FUNCTION! value which is
    // self-referentially the function itself.  This means that the paramlist
    // can be passed around as a single pointer from which a whole REBVAL
    // for the function can be found (although this value is archetypal, and
    // loses the `binding` property--which must be preserved other ways)
    //
    // The `link.meta` field of the paramlist holds a meta object (if any)
    // that describes the function.  This is read by help.
    //
    // The `misc.underlying` field of the paramlist may point to the
    // specialization whose frame should be used to set the default values
    // for the arguments during a call.  Or it will point directly to the
    // function whose paramlist should be used in the frame pushed.  This is
    // different in hijackers, adapters, and chainers.
    //
    REBARR *paramlist;

    // `body_holder` is an optimized "singular" REBSER, the size of exactly
    // one value.  This is because the information for a function body is an
    // array in the majority of function instances, and also because it can
    // standardize the native dispatcher code in the REBARR's series "misc"
    // field.  This gives two benefits: no need for a switch on the function's
    // type to figure out the dispatcher, and also to move the dispatcher out
    // of the REBVAL itself into something that can be revectored or "hooked"
    // for all instances of the function.
    //
    // PLAIN FUNCTIONS: body is a BLOCK!, the body of the function, obviously
    // NATIVES: body is "equivalent code for native" (if any) in help
    // ACTIONS: body is a WORD! for the verb of the action (OPEN, APPEND, etc)
    // SPECIALIZATIONS: body is a 1-element array containing a FRAME!
    // CALLBACKS: body a HANDLE! (REBRIN*)
    // ROUTINES: body a HANDLE! (REBRIN*)
    //
    REBARR *body_holder;
};

struct Reb_Any_Context {
    //
    // `varlist` is a Rebol Array that from 1..NUM_VARS contains REBVALs
    // representing the stored values in the context.
    //
    // As with the `paramlist` of a FUNCTION!, the varlist uses the [0]th
    // element specially.  It stores a copy of the ANY-CONTEXT! value that
    // refers to itself.
    //
    // The `keylist` is held in the varlist's Reb_Series.misc field, and it
    // may be shared with an arbitrary number of other contexts.  Changing
    // the keylist involves making a copy if it is shared.
    //
    // REB_MODULE depends on a property stored in the "meta" miscellaneous
    // field of the keylist, which is another object's-worth of data *about*
    // the module's contents (e.g. the processed header)
    //
    REBARR *varlist;

    void *unused; // for future expansion
};


// The order in which refinements are defined in a function spec may not match
// the order in which they are mentioned on a path.  As an efficiency trick,
// a word on the data stack representing a refinement usage request can be
// mutated to store the pointer to its `param` and `arg` positions, so that
// they may be returned to after the later-defined refinement has had its
// chance to take the earlier fulfillments.
//
struct Reb_Varargs {
    //
    // For as long as the VARARGS! can be used, the function it is applying
    // will be alive.  Assume that the locked paramlist won't move in memory
    // (evaluation would break if so, anyway) and hold onto the TYPESET!
    // describing the parameter.  Each time a value is fetched from the EVAL
    // then type check it for convenience.  Use ANY-VALUE! if not wanted.
    //
    // Note: could be a parameter index in the worst case scenario that the
    // array grew, revisit the rules on holding pointers into paramlists.
    //
    const REBVAL *param;

    // Similar to the param, the arg is only good for the lifetime of the
    // FRAME!...but even less so, because VARARGS! can (currently) be
    // overwritten with another value in the function frame at any point.
    // Despite this, we proxy the VALUE_FLAG_EVALUATED from the last TAKE
    // onto the argument to reflect its *argument* status.
    //
    REBVAL *arg;
};

struct Reb_Handle {
    CFUNC *code;
    void *data;
};


// Meta information in singular->link.meta
// Fild descriptor in singular->misc.fd
//
struct Reb_Library {
    REBARR *singular; // singular array holding this library value
};

typedef REBARR REBLIB;


// The general FFI direction is to move it so that it is "baked in" less,
// and represents an instance of a generalized extension mechanism (like GOB!
// should be).  On that path, a struct's internals are simplified to being
// just an array:
//
// [0] is a specification OBJECT! which contains all the information about
// the structure's layout, regardless of what offset it would find itself at
// inside of a data blob.  This includes the total size, and arrays of
// field definitions...essentially, the validated spec.  It also contains
// a HANDLE! which contains the FFI-type.
//
// [1] is the content BINARY!.  The VAL_INDEX of the binary indicates the
// offset within the struct.
//
// As an interim step, the [0] is the ordinary struct fields series as an
// ordinary BINARY!
//
struct Reb_Struct {
    REBARR *stu; // [0] is canon self value, ->misc.schema is schema
    REBSER *data; // binary data series (may be shared with other structs)
};

typedef REBARR REBSTU;

#pragma pack() // set back to default (was set to 4 at start of file)
    #include "reb-gob.h"
#pragma pack(4) // resume packing with 4

struct Reb_Gob {
    REBGOB *gob;
    REBCNT index;
};


// Reb_All is a structure type designed specifically for getting at
// the underlying bits of whichever union member is in effect inside
// the Reb_Value_Data.  This is not actually legal, although if types
// line up in unions it could be possibly be made "more legal":
//
//     http://stackoverflow.com/questions/11639947/
//
struct Reb_All {
#if defined(__LP64__) || defined(__LLP64__)
    REBCNT bits[4];
#else
    REBCNT bits[2];
#endif
};


//=////////////////////////////////////////////////////////////////////////=//
//
//  VALUE CELL DEFINITION (`struct Reb_Value`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The value is defined to have the header, "extra", and payload.  Having
// the header come first is taken advantage of by the trick for allowing
// a single REBUPT-sized value (32-bit on 32 bit builds, 64-bit on 64-bit
// builds) be examined to determine if a value is an END marker or not.
//
// Conceptually speaking, one might think of the "extra" as being part of
// the payload.  But it is broken out into a separate union.  This is because
// the `binding` property is written using common routines for several
// different types.  If the common routine picked just one of the payload
// unions to initialize, it would "disengage" the other unions.
//
// (C permits *reading* of common leading elements from another union member,
// even if that wasn't the last union used to write it.  But all bets are off
// for other unions if you *write* a leading member through another one.
// For longwinded details: http://stackoverflow.com/a/11996970/211160 )
//
// Another aspect of breaking out the "extra" is so that on 32-bit platforms,
// the starting address of the payload is on a 64-bit alignment boundary.
// See Reb_Integer, Reb_Decimal, and Reb_Typeset for examples where the 64-bit
// quantity requires things like REBDEC to have 64-bit alignment.  At time of
// writing, this is necessary for the "C-to-Javascript" emscripten build to
// work.  It's also likely preferred by x86.
//
// (Note: The reason why error-causing alignments an happen at all is due to
// the #pragma pack(4) that is put in effect at the top of this file.)
//

union Reb_Value_Extra {
    //
    // The binding will be either a REBFUN (relative to a function) or a
    // REBCTX (specific to a context).  ARRAY_FLAG_VARLIST can be
    // used to tell which it is.
    //
    // ANY-WORD!: binding is the word's binding
    //
    // ANY-ARRAY!: binding is the relativization or specifier for the REBVALs
    // which can be found inside of the frame (for recursive resolution
    // of ANY-WORD!s)
    //
    // FUNCTION!: binding is the instance data for archetypal invocation, so
    // although all the RETURN instances have the same paramlist, it is
    // the binding which is unique to the REBVAL specifying which to exit
    //
    // ANY-CONTEXT!: if a FRAME!, the binding carries the instance data from
    // the function it is for.  So if the frame was produced for an instance
    // of RETURN, the keylist only indicates the archetype RETURN.  Putting
    // the binding back together can indicate the instance.
    //
    // VARARGS!: the binding may be to a frame context and it may be to just
    // an array from which values are read.  It might also be bound to a
    // function paramlist it doesn't use, because word pickups overwrite WORD!
    // => VARARGS! in the evaluator loop...and don't reinitialize binding
    //
    REBARR *binding;

    // The remaining properties are the "leftovers" of what won't fit in the
    // payload for other types.  If those types have a quanitity that requires
    // 64-bit alignment, then that gets the priority for being in the payload,
    // with the "Extra" pointer-sized item here.

    REBSTR *key_spelling; // if typeset is key of object or function parameter
    REBDAT date; // time's payload holds the nanoseconds, this is the date
    REBCNT struct_offset; // offset for struct in the possibly shared series

    // !!! Biasing Ren-C to helping solve its technical problems led the
    // REBEVT stucture to get split up.  The "eventee" is now in the extra
    // field, while the event payload is elsewhere.  This brings about a long
    // anticipated change where REBEVTs would need to be passed around in
    // clients as REBVAL-sized entities.
    //
    // See also rebol_devreq->requestee

    union Reb_Eventee eventee;

    unsigned m0:32; // !!! significand, lowest part - see notes on Reb_Money

#if !defined(NDEBUG)
    REBUPT do_count; // used by track payloads
#endif
};

union Reb_Value_Payload {
    struct Reb_All all;

#if !defined(NDEBUG)
    struct Reb_Track track; // debug only (for void/trash, NONE!, LOGIC!, BAR!)
#endif

    REBUNI character; // It's CHAR! (for now), but 'char' is a C keyword
    REBI64 integer;
    REBDEC decimal;

    struct Reb_Pair pair;
    struct Reb_Money money;
    struct Reb_Handle handle;
    struct Reb_Time time;
    struct Reb_Tuple tuple;
    struct Reb_Datatype datatype;
    struct Reb_Typeset typeset;

    struct Reb_Library library;
    struct Reb_Struct structure; // It's STRUCT!, but 'struct' is a C keyword

    struct Reb_Event event;
    struct Reb_Gob gob;

    // These use `specific` or `relative` in `binding`, based on IS_RELATIVE()

    struct Reb_Any_Word any_word;
    struct Reb_Any_Series any_series;
    struct Reb_Function function;
    struct Reb_Any_Context any_context;
    struct Reb_Varargs varargs;
};

struct Reb_Value
{
    struct Reb_Header header;
    union Reb_Value_Extra extra;
    union Reb_Value_Payload payload;
};



//=////////////////////////////////////////////////////////////////////////=//
//
//  END marker (not a value type, only writes `struct Reb_Value_Flags`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Historically Rebol arrays were always one value longer than their maximum
// content, and this final slot was used for a special REBVAL called END!.
// Like a null terminator in a C string, it was possible to start from one
// point in the series and traverse to find the end marker without needing
// to maintain a count.  Rebol series store their length also--but it's
// faster and more general to use the terminator.
//
// Ren-C changed this so that end is not a data type, but rather seeing a
// header slot with the lowest bit set to 0.  (See NOT_END_MASK for
// an explanation of this choice.)  The upshot is that a data structure
// designed to hold Rebol arrays is able to terminate an array at full
// capacity with a pointer-sized integer with the lowest 2 bits clear, and
// use the rest of the bits for other purposes.  (See WRITABLE_MASK_DEBUG
// for why it's the low 2 bits and not just the lowest bit.)
//
// This means not only is a full REBVAL not needed to terminate, the sunk cost
// of an existing 32-bit or 64-bit number (depending on platform) can be used
// to avoid needing even 1/4 of a REBVAL for a header to terminate.  (See the
// `size` field in `struct Reb_Chunk` from %sys-stack.h for a simple example
// of the technique.)
//
// !!! Because Rebol Arrays (REBARR) have both a length and a terminator, it
// is important to keep these in sync.  R3-Alpha sought to give code the
// freedom to work with unterminated arrays if the cost of writing terminators
// was not necessary.  Ren-C pushed back against this to try and be more
// uniform to get the invariants under control.  A formal balance is still
// being sought of when terminators will be required and when they will not.
//
// The debug build puts REB_MAX in the type slot of a REB_END, to help to
// distinguish it from the 0 that signifies an unset TRASH.  This means that
// any writable value can be checked to ensure it is an actual END marker
// and not "uninitialized".  This trick can only be used so long as REB_MAX
// is 63 or smaller (ensured by an assertion at startup ATM.
//

#define END_CELL \
    c_cast(const REBVAL*, &PG_End_Cell)

#define IS_END_MACRO(v) \
    LOGICAL((v)->header.bits % 2 == 0) // == NOT(bits & NOT_END_MASK)

#ifdef NDEBUG
    #define IS_END(v) \
        IS_END_MACRO(v)

    inline static void SET_END(RELVAL *v) {
        v->header.bits = 0;
    }
#else
    // Note: These must be macros (that don't need IS_END_Debug or
    // SET_END_Debug defined until used at a callsite) because %tmp-funcs.h
    // cannot be included until after REBSER and other definitions that
    // depend on %sys-rebval.h have been defined.  (Or they could be manually
    // forward-declared here.)
    //
    #define IS_END(v) \
        IS_END_Debug((v), __FILE__, __LINE__)

    #define SET_END(v) \
        SET_END_Debug((v), __FILE__, __LINE__)
#endif

#define NOT_END(v) \
    NOT(IS_END(v))


//=////////////////////////////////////////////////////////////////////////=//
//
//  REBVAL ("fully specified" value) and RELVAL ("possibly relative" value)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// A relative value is the identical struct to Reb_Value, but is allowed to
// have the relative bit set.  Hence a relative value pointer can point to a
// specific value, but a relative word or array cannot be pointed to by a
// plain REBVAL*.  The RELVAL-vs-REBVAL distinction is purely commentary
// in the C build, but the C++ build makes REBVAL a type derived from RELVAL.
//
// RELVAL exists to help quarantine the bit patterns for relative words into
// the deep-copied-body of the function they are for.  To actually look them
// up, they must be paired with a FRAME! matching the actual instance of the
// running function on the stack they correspond to.  Once made specific,
// a word may then be freely copied into any REBVAL slot.
//
// In addition to ANY-WORD!, an ANY-ARRAY! can also be relative, if it is
// part of the deep-copied function body.  The reason that arrays must be
// relative too is in case they contain relative words.  If they do, then
// recursion into them must carry forward the resolving "specifier" pointer
// to be combined with any relative words that are seen later.
//

#ifdef __cplusplus
    struct Reb_Specific_Value : public Reb_Value {
    #if !defined(NDEBUG)
        //
        // In C++11, it is now formally legal to add constructors to types
        // without interfering with their "standard layout" properties, or
        // making them uncopyable with memcpy(), etc.  For the rules, see:
        //
        //     http://stackoverflow.com/a/7189821/211160
        //
        // In the debug C++ build there is an extra check of writability. This
        // makes sure that stack variables holding REBVAL are marked with that.
        // It also means that the check can only be performed by the C++ build,
        // otherwise there would need to be a manual set of this bit on every
        // stack variable.
        //
        Reb_Specific_Value () {
            header.bits = CELL_MASK | VALUE_FLAG_WRITABLE_DEBUG;
        }
    #endif
    };
#endif

inline static REBOOL IS_RELATIVE(const RELVAL *v) {
    return LOGICAL(v->header.bits & VALUE_FLAG_RELATIVE);
}

#if defined(__cplusplus)
    //
    // Take special advantage of the fact that C++ can help catch when we are
    // trying to see if a REBVAL is specific or relative (it will always
    // be specific, so the call is likely in error).  In the C build, they
    // are the same type so there will be no error.
    //
    REBOOL IS_RELATIVE(const REBVAL *v);
#endif

#define IS_SPECIFIC(v) \
    NOT(IS_RELATIVE(v))

inline static REBFUN *VAL_RELATIVE(const RELVAL *v) {
    assert(IS_RELATIVE(v));
    //assert(NOT(GET_ARR_FLAG(v->extra.binding, ARRAY_FLAG_VARLIST)));
    return cast(REBFUN*, v->extra.binding);
}

inline static REBCTX *VAL_SPECIFIC_COMMON(const RELVAL *v) {
    assert(IS_SPECIFIC(v));
    //assert(
    //    v->extra.binding == SPECIFIED
    //    || GET_ARR_FLAG(v->extra.binding, ARRAY_FLAG_VARLIST)
    //);
    return cast(REBCTX*, v->extra.binding);
}

#ifdef NDEBUG
    #define VAL_SPECIFIC(v) \
        VAL_SPECIFIC_COMMON(v)
#else
    #define VAL_SPECIFIC(v) \
        VAL_SPECIFIC_Debug(v)
#endif

// When you have a RELVAL* (e.g. from a REBARR) that you "know" to be specific,
// the KNOWN macro can be used for that.  Checks to make sure in debug build.
//
// Use for: "invalid conversion from 'Reb_Value*' to 'Reb_Specific_Value*'"

inline static const REBVAL *const_KNOWN(const RELVAL *value) {
    assert(IS_SPECIFIC(value));
    return cast(const REBVAL*, value); // we asserted it's actually specific
}

inline static REBVAL *KNOWN(RELVAL *value) {
    assert(IS_SPECIFIC(value));
    return cast(REBVAL*, value); // we asserted it's actually specific
}

inline static const RELVAL *const_REL(const REBVAL *v) {
    return cast(const RELVAL*, v); // cast w/input restricted to REBVAL
}

inline static RELVAL *REL(REBVAL *v) {
    return cast(RELVAL*, v); // cast w/input restricted to REBVAL
}

#ifdef NDEBUG
    #define SPECIFIED NULL
#else
    #define SPECIFIED \
        cast(REBCTX*, 0xF10F10F1) // helps catch uninitialized locations
#endif

// !!! temporary - used to document any sites where one is not sure if the
// value is specific, to aid in finding them to review
//
#define GUESSED SPECIFIED

// This can be used to turn a RELVAL into a REBVAL.  If the RELVAL is
// indeed relative and needs to be made specific to be put into the
// REBVAL, then the specifier is used to do that.  Debug builds assert
// that the function in the specifier indeed matches the target in
// the relative value (because relative values in an array may only
// be relative to the function that deep copied them, and that is the
// only kind of specifier you can use with them).
//
// NOTE: The reason this is written to specifically intialize the `specific`
// through the union member of the remaining type is to stay on the right
// side of the standard.  While *reading* a common leading field out of
// different union members is legal regardless of who wrote it last,
// *writing* a common leading field will invalidate the ensuing fields of
// other union types besides the one it was written through (!)  See notes
// in Reb_Value's structure definition.
//
inline static void COPY_VALUE_CORE(
    REBVAL *dest,
    const RELVAL *src,
    REBCTX *specifier
){
    if (src->header.bits & VALUE_FLAG_RELATIVE) {
        dest->header.bits
            = src->header.bits & ~cast(REBUPT, VALUE_FLAG_RELATIVE);
        dest->extra.binding = cast(REBARR*, specifier);
    }
    else {
        dest->header = src->header;
        dest->extra.binding = src->extra.binding;
    }
    dest->payload = src->payload;
}


#ifdef NDEBUG
    #define COPY_VALUE(dest,src,specifier) \
        COPY_VALUE_CORE(SINK(dest),(src),(specifier))
#else
    #define COPY_VALUE(dest,src,specifier) \
        COPY_VALUE_Debug(SINK(dest),(src),(specifier))
#endif

#ifdef NDEBUG
    #define ASSERT_NO_RELATIVE(array,deep) NOOP
#else
    #define ASSERT_NO_RELATIVE(array,deep) \
        Assert_No_Relative((array),(deep))
#endif

#pragma pack() // set back to default (was set to 4 at start of file)