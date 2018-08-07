//
//  File: %sys-value.h
//  Summary: {any-value! defs AFTER %tmp-internals.h (see: %sys-rebval.h)}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2018 Rebol Open Source Contributors
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
// This file provides basic accessors for value types.  Because these
// accessors operate on REBVAL (or RELVAL) pointers, the inline functions need
// the complete struct definition available from all the payload types.
//
// See notes in %sys-rebval.h for the definition of the REBVAL structure.
//
// An attempt is made to group the accessors in sections.  Some functions are
// defined in %c-value.c for the sake of the grouping.
//
// While some REBVALs are in C stack variables, most reside in the allocated
// memory block for a Rebol series.  The memory block for a series can be
// resized and require a reallocation, or it may become invalid if the
// containing series is garbage-collected.  This means that many pointers to
// REBVAL are unstable, and could become invalid if arbitrary user code
// is run...this includes values on the data stack, which is implemented as
// a series under the hood.  (See %sys-stack.h)
//
// A REBVAL in a C stack variable does not have to worry about its memory
// address becoming invalid--but by default the garbage collector does not
// know that value exists.  So while the address may be stable, any series
// it has in the payload might go bad.  Use PUSH_GC_GUARD() to protect a
// stack variable's payload, and then DROP_GC_GUARD() when the protection
// is not needed.  (You must always drop the most recently pushed guard.)
//
// For a means of creating a temporary array of GC-protected REBVALs, see
// the "chunk stack" in %sys-stack.h.  This is used when building function
// argument frames, which means that the REBVAL* arguments to a function
// accessed via ARG() will be stable as long as the function is running.
//


//=////////////////////////////////////////////////////////////////////////=//
//
//  DEBUG PROBE <== **THIS IS VERY USEFUL**
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The PROBE macro can be used in debug builds to mold a REBVAL much like the
// Rebol `probe` operation.  But it's actually polymorphic, and if you have
// a REBSER*, REBCTX*, or REBARR* it can be used with those as well.  In C++,
// you can even get the same value and type out as you put in...just like in
// Rebol, permitting things like `return PROBE(Make_Some_Series(...));`
//
// In order to make it easier to find out where a piece of debug spew is
// coming from, the file and line number will be output as well.
//
// Note: As a convenience, PROBE also flushes the `stdout` and `stderr` in
// case the debug build was using printf() to output contextual information.
//

#if defined(DEBUG_HAS_PROBE)
    #ifdef CPLUSPLUS_11
        template <typename T>
        T Probe_Cpp_Helper(T v, const char *file, int line) {
            return cast(T, Probe_Core_Debug(v, file, line));
        }

        #define PROBE(v) \
            Probe_Cpp_Helper((v), __FILE__, __LINE__) // passes input as-is
    #else
        #define PROBE(v) \
            Probe_Core_Debug((v), __FILE__, __LINE__) // just returns void* :(
    #endif
#elif !defined(NDEBUG) // don't cause compile time error on PROBE()
    #define PROBE(v) \
        do { \
            printf("DEBUG_HAS_PROBE disabled %s %d\n", __FILE__, __LINE__); \
            fflush(stdout); \
        } while (0)
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  TRACKING PAYLOAD <== **THIS IS VERY USEFUL**
//
//=////////////////////////////////////////////////////////////////////////=//
//
// In the debug build, "Trash" cells (NODE_FLAG_FREE) can use their payload to
// store where and when they were initialized.  This also applies to some
// datatypes like BLANK!, BAR!, LOGIC!, or void--since they only use their
// header bits, they can also use the payload for this in the debug build.
//
// (Note: The release build does not canonize unused bits of payloads, so
// they are left as random data in that case.)
//
// View this information in the debugging watchlist under the `track` union
// member of a value's payload.  It is also reported by panic().
//

#if defined(DEBUG_TRACK_CELLS)
    #if defined(DEBUG_COUNT_TICKS) && defined(DEBUG_TRACK_EXTEND_CELLS)
        #define TOUCH_CELL(c) \
            ((c)->touch = TG_Tick)
    #endif

    inline static void Set_Track_Payload_Extra_Debug(
        RELVAL *c,
        const char *file,
        int line
    ){
      #ifdef DEBUG_TRACK_EXTEND_CELLS // cell is made bigger to hold it
        c->track.file = file;
        c->track.line = line;

        #ifdef DEBUG_COUNT_TICKS
            c->tick = TG_Tick;
            c->touch = 0;
        #endif
      #else // in space that is overwritten for cells that fill in payloads 
        c->payload.track.file = file;
        c->payload.track.line = line;
          
        #ifdef DEBUG_COUNT_TICKS
            c->extra.tick = TG_Tick;
        #endif
      #endif
    }

    #define TRACK_CELL_IF_DEBUG(c,file,line) \
        Set_Track_Payload_Extra_Debug((c), (file), (line))
#else
    #define TRACK_CELL_IF_DEBUG(c,file,line) \
        NOOP
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  VALUE "KIND" (1 out of 64 different foundational types)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Every value has 6 bits reserved for its VAL_TYPE().  The reason only 6
// are used is because low-level TYPESET!s are only 64-bits (so they can fit
// into a REBVAL payload, along with a key symbol to represent a function
// parameter).  If there were more types, they couldn't be flagged in a
// typeset that fit in a REBVAL under that constraint.
//
// VAL_TYPE() should obviously not be called on uninitialized memory.  But
// it should also not be called on an END marker, as those markers only
// guarantee the low bit as having Rebol-readable-meaning.  In debug builds,
// this is asserted by VAL_TYPE_Debug.
//

#define FLAGIT_KIND(t) \
    (cast(REBU64, 1) << (t)) // makes a 64-bit bitflag


// While inline vs. macro doesn't usually matter much, debug builds won't
// inline this, and it's called *ALL* the time.  Since it doesn't repeat its
// argument, it's not worth it to make it a function for slowdown caused.
// Also, don't bother checking using the `cast()` template in C++.
//
// !!! Technically this is wasting two bits in the header, because there are
// only 64 types that fit in a type bitset.  Yet the sheer commonness of
// this operation makes bit masking expensive...and choosing the number of
// types based on what fits in a 64-bit mask is not necessarily the most
// future-proof concept in the first place.  Use a full byte for speed.
//
#define VAL_TYPE_RAW(v) \
    cast(enum Reb_Kind, const_KIND_BYTE(v))

#ifdef NDEBUG
    #define VAL_TYPE(v) \
        VAL_TYPE_RAW(v)
#else
    inline static enum Reb_Kind VAL_TYPE_Debug(
        const RELVAL *v, const char *file, int line
    ){
        // VAL_TYPE is called *a lot*, and this makes it a great place to do
        // sanity checks in the debug build.  But a debug build will not
        // inline this function, and makes *no* optimizations.  Using no
        // stack space e.g. no locals) is ideal.

        if (VAL_TYPE_RAW(v) == REB_0) {
            printf("VAL_TYPE() called on END marker\n");
            panic_at (v, file, line);
        }

        if (
            (v->header.bits & (
                NODE_FLAG_CELL
                | NODE_FLAG_FREE
                | TRASH_FLAG_UNREADABLE_IF_DEBUG
            )) == NODE_FLAG_CELL
        ){
            return VAL_TYPE_RAW(v);
        }

        if (not (v->header.bits & NODE_FLAG_CELL)) {
            printf("VAL_TYPE() called on non-cell\n");
            panic_at (v, file, line);
        }

        if (v->header.bits & NODE_FLAG_FREE) {
            printf("VAL_TYPE() called on invalid cell--marked FREE\n");
            panic_at (v, file, line);
        }

        assert(v->header.bits & TRASH_FLAG_UNREADABLE_IF_DEBUG);

        if (VAL_TYPE_RAW(v) == REB_MAX_PLUS_TWO_TRASH) {
            printf("VAL_TYPE() called on trash cell\n");
            panic_at (v, file, line);
        }

        if (VAL_TYPE_RAW(v) == REB_BLANK) {
            printf("VAL_TYPE() called on unreadable BLANK!\n");
            panic_at (v, file, line);
        }

        // Hopefully rare case... some other type that is using the same
        // 24th-from-the-left bit as TRASH_FLAG_UNREADABLE_IF_DEBUG, and it's
        // set, but doesn't mean the type is actually unreadable.  Avoid
        // making this a common case, as it slows the debug build.
        // 
        return VAL_TYPE_RAW(v);
    }

    #define VAL_TYPE(v) \
        VAL_TYPE_Debug((v), __FILE__, __LINE__)
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  VALUE FLAGS
//
//=////////////////////////////////////////////////////////////////////////=//
//
// VALUE_FLAG_XXX flags are applicable to all types.  Type-specific flags are
// named things like TYPESET_FLAG_XXX or WORD_FLAG_XXX and only apply to the
// type that they reference.  Both use these XXX_VAL_FLAG accessors.
//

#ifdef NDEBUG
    #define SET_VAL_FLAGS(v,f) \
        (v)->header.bits |= (f)

    #ifdef CPLUSPLUS_11
        //
        // In the C++ release build we sanity check that only one bit is set.
        // The assert is done at compile-time, you must use a constant flag.
        // If you need dynamic flag checking, use GET_VAL_FLAGS even for one.
        //
        // Note this is not included as a runtime assert because it is costly,
        // and it's not included in the debug build because the flags are
        // "contaminated" with additional data that's hard to mask out at
        // compile-time due to the weirdness of CLEAR_8_RIGHT_BITS.  This
        // pattern does not catch bad flag checks in asserts.  Review.

        template <uintptr_t f>
        inline static void SET_VAL_FLAG_cplusplus(RELVAL *v) {
            static_assert(
                f and (f & (f - 1)) == 0, // only one bit is set
                "use SET_VAL_FLAGS() to set multiple bits"
            );
            v->header.bits |= f;
        }
        #define SET_VAL_FLAG(v,f) \
            SET_VAL_FLAG_cplusplus<f>(v)
        
        template <uintptr_t f>
        inline static REBOOL GET_VAL_FLAG_cplusplus(const RELVAL *v) {
            static_assert(
                f and (f & (f - 1)) == 0, // only one bit is set
                "use ANY_VAL_FLAGS() or ALL_VAL_FLAGS() to test multiple bits"
            );
            return did (v->header.bits & f);
        }
        #define GET_VAL_FLAG(v,f) \
            GET_VAL_FLAG_cplusplus<f>(v)
    #else
        #define SET_VAL_FLAG(v,f) \
            SET_VAL_FLAGS((v), (f))

        #define GET_VAL_FLAG(v, f) \
            (did ((v)->header.bits & (f)))
    #endif

    #define ANY_VAL_FLAGS(v,f) \
        (((v)->header.bits & (f)) != 0)

    #define ALL_VAL_FLAGS(v,f) \
        (((v)->header.bits & (f)) == (f))

    #define CLEAR_VAL_FLAGS(v,f) \
        ((v)->header.bits &= ~(f))

    #define CLEAR_VAL_FLAG(v,f) \
        CLEAR_VAL_FLAGS((v), (f))

    #define CHECK_VALUE_FLAGS_EVIL_MACRO_DEBUG(flags) \
        NOOP
#else
    // For safety in the debug build, all the type-specific flags include a
    // type (or type representing a category) as part of the flag.  This type
    // is checked first, and then masked out to use the single-bit-flag value
    // which is intended.
    //
    // But flag testing routines are called *a lot*, and debug builds do not
    // inline functions.  So it's worth doing a sketchy macro so this somewhat
    // borderline assert doesn't wind up taking up 20% of the debug's runtime.
    //
    #define CHECK_VALUE_FLAGS_EVIL_MACRO_DEBUG(flags) \
        enum Reb_Kind category = cast(enum Reb_Kind, SECOND_BYTE(flags)); \
        assert(kind <= REB_MAX + 1); /* REB_0 is end, REB_MAX is null */ \
        if (category != REB_0) { \
            if (kind != category) { \
                if (category == REB_WORD) \
                    assert(ANY_WORD_KIND(kind)); \
                else if (category == REB_OBJECT) \
                    assert(ANY_CONTEXT_KIND(kind)); \
                else \
                    assert(FALSE); \
            } \
            SECOND_BYTE(flags) = 0; \
        } \

    inline static void SET_VAL_FLAGS(RELVAL *v, uintptr_t f) {
        enum Reb_Kind kind = VAL_TYPE_RAW(v);
        CHECK_VALUE_FLAGS_EVIL_MACRO_DEBUG(f);
        v->header.bits |= f;
    }

    inline static void SET_VAL_FLAG(RELVAL *v, uintptr_t f) {
        enum Reb_Kind kind = VAL_TYPE_RAW(v);
        CHECK_VALUE_FLAGS_EVIL_MACRO_DEBUG(f);
        v->header.bits |= f;
    }

    inline static REBOOL GET_VAL_FLAG(const RELVAL *v, uintptr_t f) {
        enum Reb_Kind kind = VAL_TYPE_RAW(v);
        CHECK_VALUE_FLAGS_EVIL_MACRO_DEBUG(f);
        return did (v->header.bits & f);
    }

    inline static REBOOL ANY_VAL_FLAGS(const RELVAL *v, uintptr_t f) {
        enum Reb_Kind kind = VAL_TYPE_RAW(v);
        CHECK_VALUE_FLAGS_EVIL_MACRO_DEBUG(f);
        return (v->header.bits & f) != 0;
    }

    inline static REBOOL ALL_VAL_FLAGS(const RELVAL *v, uintptr_t f) {
        enum Reb_Kind kind = VAL_TYPE_RAW(v);
        CHECK_VALUE_FLAGS_EVIL_MACRO_DEBUG(f);
        return (v->header.bits & f) == f;
    }

    inline static void CLEAR_VAL_FLAGS(RELVAL *v, uintptr_t f) {
        enum Reb_Kind kind = VAL_TYPE_RAW(v);
        CHECK_VALUE_FLAGS_EVIL_MACRO_DEBUG(f);
        v->header.bits &= ~f;
    }

    inline static void CLEAR_VAL_FLAG(RELVAL *v, uintptr_t f) {
        enum Reb_Kind kind = VAL_TYPE_RAW(v);
        CHECK_VALUE_FLAGS_EVIL_MACRO_DEBUG(f);
        assert(f and (f & (f - 1)) == 0); // checks that only one bit is set
        v->header.bits &= ~f;
    }
#endif

#define NOT_VAL_FLAG(v,f) \
    cast(REBOOL, not GET_VAL_FLAG((v), (f)))


//=////////////////////////////////////////////////////////////////////////=//
//
//  CELL WRITABILITY
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Asserting writiablity helps avoid very bad catastrophies that might ensue
// if "implicit end markers" could be overwritten.  These are the ENDs that
// are actually other bitflags doing double duty inside a data structure, and
// there is no REBVAL storage backing the position.
//
// (A fringe benefit is catching writes to other unanticipated locations.)
//

#if defined(DEBUG_CELL_WRITABILITY)
    //
    // In the debug build, functions aren't inlined, and the overhead actually
    // adds up very quickly of getting the 3 parameters passed in.  Run the
    // risk of repeating macro arguments to speed up this critical test.
    //
    #define ASSERT_CELL_WRITABLE_EVIL_MACRO(c,file,line) \
        if (not ((c)->header.bits & NODE_FLAG_CELL)) { \
            printf("Non-cell passed to cell writing routine\n"); \
            panic_at ((c), (file), (line)); \
        } \
        else if (not ((c)->header.bits & NODE_FLAG_NODE)) { \
            printf("Non-node passed to cell writing routine\n"); \
            panic_at ((c), (file), (line)); \
        } else if (\
            (c)->header.bits & (CELL_FLAG_PROTECTED | NODE_FLAG_FREE) \
        ){ \
            printf("Protected/free cell passed to writing routine\n"); \
            panic_at ((c), (file), (line)); \
        }
#else
    #define ASSERT_CELL_WRITABLE_EVIL_MACRO(c,file,line) \
        NOOP
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  CELL HEADERS AND PREPARATION
//
//=////////////////////////////////////////////////////////////////////////=//
//
// RESET_VAL_HEADER clears out the header of *most* bits, setting it to a
// new type.  The type takes up the full "rightmost" byte of the header,
// despite the fact it only needs 6 bits.  However, the performance advantage
// of not needing to mask to do VAL_TYPE() is worth it...also there may be a
// use for 256 types (although type bitsets are only 64-bits at the moment)
//
// The value is expected to already be "pre-formatted" with the NODE_FLAG_CELL
// bit, so that is left as-is.  It is also expected that CELL_FLAG_STACK has
// been set if the value is stack-based (e.g. on the C stack or in a frame),
// so that is left as-is also.
//

inline static REBVAL *RESET_VAL_HEADER_EXTRA_Core(
    RELVAL *v,
    enum Reb_Kind kind,
    uintptr_t extra

  #if defined(DEBUG_CELL_WRITABILITY)
  , const char *file
  , int line
  #endif
){
    ASSERT_CELL_WRITABLE_EVIL_MACRO(v, file, line);

    // The debug build puts some extra type information onto flags
    // which needs to be cleared out.  (e.g. ACTION_FLAG_XXX has the bit
    // pattern for REB_ACTION inside of it, to help make sure that flag
    // doesn't get used with things that aren't actions.)
    //
    CHECK_VALUE_FLAGS_EVIL_MACRO_DEBUG(extra);

    v->header.bits &= CELL_MASK_PERSIST;
    v->header.bits |= FLAG_KIND_BYTE(kind) | extra;
    return cast(REBVAL*, v);
}

#if defined(DEBUG_CELL_WRITABILITY)
    #define RESET_VAL_HEADER_EXTRA(v,kind,extra) \
        RESET_VAL_HEADER_EXTRA_Core((v), (kind), (extra), __FILE__, __LINE__)
#else
    #define RESET_VAL_HEADER_EXTRA(v,kind,extra) \
        RESET_VAL_HEADER_EXTRA_Core((v), (kind), (extra))
#endif

#define RESET_VAL_HEADER(v,kind) \
    RESET_VAL_HEADER_EXTRA((v), (kind), 0)

#ifdef DEBUG_TRACK_CELLS
    //
    // VAL_RESET is a variant of RESET_VAL_HEADER_EXTRA that actually
    // overwrites the payload with tracking information.  It should not be
    // used if the intent is to preserve the payload and extra, and is
    // wasteful if you're just going to overwrite them immediately afterward.
    //
    inline static REBVAL *RESET_VAL_CELL_Debug(
        RELVAL *out,
        enum Reb_Kind kind,
        uintptr_t extra,
        const char *file,
        int line
    ){
      #ifdef DEBUG_CELL_WRITABILITY
        RESET_VAL_HEADER_EXTRA_Core(out, kind, extra, file, line);
      #else
        RESET_VAL_HEADER_EXTRA(out, kind, extra);
      #endif

        TRACK_CELL_IF_DEBUG(out, file, line);
        return cast(REBVAL*, out);
    }

    #define RESET_VAL_CELL(out,kind,extra) \
        RESET_VAL_CELL_Debug((out), (kind), (extra), __FILE__, __LINE__)
#else
    #define RESET_VAL_CELL(out,kind,extra) \
       RESET_VAL_HEADER_EXTRA((out), (kind), (extra))
#endif


// This is another case where the debug build doesn't inline functions, and
// for such central routines the overhead of passing 3 args is on the radar.
// Run the risk of repeating macro args to speed up this critical check.
//
#define ALIGN_CHECK_CELL_EVIL_MACRO(c,file,line) \
    if (cast(uintptr_t, (c)) % sizeof(REBI64) != 0) { \
        printf( \
            "Cell address %p not aligned to %d bytes\n", \
            cast(const void*, (c)), \
            cast(int, sizeof(REBI64)) \
        ); \
        panic_at ((c), file, line); \
    }

#define CELL_MASK_NON_STACK \
    (NODE_FLAG_NODE | NODE_FLAG_CELL)

inline static void Prep_Non_Stack_Cell_Core(
    RELVAL *c

  #if defined(DEBUG_TRACK_CELLS)
  , const char *file
  , int line
  #endif
){
  #ifdef DEBUG_MEMORY_ALIGN
    ALIGN_CHECK_CELL_EVIL_MACRO(c, file, line);
  #endif

    c->header.bits = CELL_MASK_NON_STACK;
    TRACK_CELL_IF_DEBUG(cast(RELVAL*, c), file, line);
}

#if defined(DEBUG_TRACK_CELLS)
    #define Prep_Non_Stack_Cell(c) \
        Prep_Non_Stack_Cell_Core((c), __FILE__, __LINE__)
#else
    #define Prep_Non_Stack_Cell(c) \
        Prep_Non_Stack_Cell_Core(c)
#endif

#define CELL_MASK_STACK \
    (NODE_FLAG_NODE | NODE_FLAG_CELL | CELL_FLAG_STACK)

inline static void Prep_Stack_Cell_Core(
    RELVAL *c

  #if defined(DEBUG_TRACK_CELLS)
  , const char *file
  , int line
  #endif
){
  #ifdef DEBUG_MEMORY_ALIGN
    ALIGN_CHECK_CELL_EVIL_MACRO(c, file, line);
  #endif

    c->header.bits = CELL_MASK_STACK | FLAG_KIND_BYTE(REB_MAX_PLUS_TWO_TRASH);
    TRACK_CELL_IF_DEBUG(cast(RELVAL*, c), file, line);
}

#if defined(DEBUG_TRACK_CELLS)
    #define Prep_Stack_Cell(c) \
        Prep_Stack_Cell_Core((c), __FILE__, __LINE__)
#else
    #define Prep_Stack_Cell(c) \
        Prep_Stack_Cell_Core(c)
#endif


inline static void CHANGE_VAL_TYPE_BITS(RELVAL *v, enum Reb_Kind kind) {
    //
    // Note: Only use if you are sure the new type payload is in sync with
    // the type and bits (e.g. changing ANY-WORD! to another ANY-WORD!).
    // Otherwise the value-specific flags might be misinterpreted.
    //
    ASSERT_CELL_WRITABLE_EVIL_MACRO(v, __FILE__, __LINE__);
    KIND_BYTE(v) = kind;
}


//=////////////////////////////////////////////////////////////////////////=//
//
//  TRASH CELLS
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Trash is a cell (marked by NODE_FLAG_CELL) with NODE_FLAG_FREE set.  To
// prevent it from being inspected while it's in an invalid state, VAL_TYPE
// used on a trash cell will assert in the debug build.
//
// The garbage collector is not tolerant of trash.
//

#if defined(DEBUG_TRASH_MEMORY)
    inline static void Set_Trash_Debug(
        RELVAL *v

      #ifdef DEBUG_TRACK_CELLS
      , const char *file
      , int line
      #endif
    ){
        ASSERT_CELL_WRITABLE_EVIL_MACRO(v, file, line);

        v->header.bits &= CELL_MASK_PERSIST;
        v->header.bits |=
            TRASH_FLAG_UNREADABLE_IF_DEBUG
            | FLAG_KIND_BYTE(REB_MAX_PLUS_TWO_TRASH);

        TRACK_CELL_IF_DEBUG(v, file, line);
    }

    #define TRASH_CELL_IF_DEBUG(v) \
        Set_Trash_Debug((v), __FILE__, __LINE__)

    inline static REBOOL IS_TRASH_DEBUG(const RELVAL *v) {
        assert(v->header.bits & NODE_FLAG_CELL);
        return VAL_TYPE_RAW(v) == REB_MAX_PLUS_TWO_TRASH;
    }
#else
    #define TRASH_CELL_IF_DEBUG(v) \
        NOOP
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  END marker (not a value type, only writes `struct Reb_Value_Flags`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Historically Rebol arrays were always one value longer than their maximum
// content, and this final slot was used for a REBVAL type called END!.
// Like a null terminator in a C string, it was possible to start from one
// point in the series and traverse to find the end marker without needing
// to look at the length (though the length in the series header is maintained
// in sync, also).
//
// Ren-C changed this so that END is not a user-exposed data type, and that
// it's not a requirement for the byte sequence containing the end byte be
// the full size of a cell.  The type byte (which is 0 for an END) lives in
// the second byte, hence two bytes are sufficient to indicate a terminator.
//
// VAL_TYPE() and many other operations will panic if they are used on an END
// cell.  Yet the special unwritable system value END is the size of a REBVAL,
// but does not carry NODE_FLAG_CELL.  Since it is a node, it can be more
// useful to return from routines that return REBVAL* than a null, because it
// can have its header dereferenced to check its type in a single test...
// as VAL_TYPE_OR_0() will return REB_0 for the system END marker.  (It's
// actually possible if you're certain you have a NODE_FLAG_CELL to know that
// the type of an end marker is REB_0, but one can rarely exploit that.)
//

#define END_NODE \
    cast(const REBVAL*, &PG_End_Node) // rebEND is char*, not REBVAL* aligned!

#if defined(DEBUG_TRACK_CELLS) || defined(DEBUG_CELL_WRITABILITY)
    inline static REBVAL *SET_END_Debug(
        RELVAL *v

      #if defined(DEBUG_TRACK_CELLS) || defined(DEBUG_CELL_WRITABILITY)
      , const char *file
      , int line
      #endif
    ){
        ASSERT_CELL_WRITABLE_EVIL_MACRO(v, file, line);

        SECOND_BYTE(v->header) = REB_0; // only thing done in release build

        TRACK_CELL_IF_DEBUG(v, file, line);
        return cast(REBVAL*, v);
    }

    #define SET_END(v) \
        SET_END_Debug((v), __FILE__, __LINE__)
#else
    inline static REBVAL *SET_END(RELVAL *v) {
        SECOND_BYTE(v->header) = 0; // needs to be a prepared cell
        return cast(REBVAL*, v);
    }
#endif

#ifdef NDEBUG
    #define IS_END(p) \
        (cast(const REBYTE*, p)[1] == REB_0)
#else
    inline static REBOOL IS_END_Debug(
        const void *p, // may not have NODE_FLAG_CELL, may be short as 2 bytes
        const char *file,
        int line
    ){
        if (cast(const REBYTE*, p)[0] & 0x40) { // e.g. NODE_FLAG_FREE
            printf("NOT_END() called on garbage\n");
            panic_at(p, file, line);
        }

        if (cast(const REBYTE*, p)[1] == REB_0)
            return true;

        if (not (cast(const REBYTE*, p)[0] & 0x01)) { // e.g. NODE_FLAG_CELL
            printf("IS_END() found non-END pointer that's not a cell\n");
            panic_at(p, file, line);
        }

        return false;
    }

  #ifdef CPLUSPLUS_11
    //
    // Disable testing a RELVAL marked that it can't be END for endness-in
    // the C++ build.  (ordinary version takes a RELVAL.)
    //
    // !!! This could probably be tweaked to give a better message with
    // type_traits and SFINAE, these just give a linker error
    //
    REBOOL IS_END_Debug(const_RELVAL_NO_END_PTR v, const char* file, int line);
  #endif

    #define IS_END(v) \
        IS_END_Debug((v), __FILE__, __LINE__)
#endif

#define NOT_END(v) \
    (not IS_END(v))


//=////////////////////////////////////////////////////////////////////////=//
//
//  RELATIVE AND SPECIFIC VALUES
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Some value types use their `->extra` field in order to store a pointer to
// a REBNOD which constitutes their notion of "binding".
//
// At time of writing, this can be either a pointer to EMPTY_ARRAY (which
// indicates UNBOUND), or to a function's paramlist (which indicates a
// relative binding), or to a context's varlist (which indicates a specific
// binding.)
//
// The ordering of %types.r is chosen specially so that all bindable types
// are at lower values than the unbindable types.
//

// An ANY-WORD! is relative if it refers to a local or argument of a function,
// and has its bits resident in the deep copy of that function's body.
//
// An ANY-ARRAY! in the deep copy of a function body must be relative also to
// the same function if it contains any instances of such relative words.
//
inline static REBOOL IS_RELATIVE(const RELVAL *v) {
    if (Not_Bindable(v) or not v->extra.binding)
        return false; // INTEGER! and other types are inherently "specific"
    return GET_SER_FLAG(v->extra.binding, ARRAY_FLAG_PARAMLIST);
}

#if defined(__cplusplus) && __cplusplus >= 201103L
    //
    // Take special advantage of the fact that C++ can help catch when we are
    // trying to see if a REBVAL is specific or relative (it will always
    // be specific, so the call is likely in error).  In the C build, they
    // are the same type so there will be no error.
    //
    REBOOL IS_RELATIVE(const REBVAL *v);
#endif

#define IS_SPECIFIC(v) \
    cast(REBOOL, not IS_RELATIVE(v))

inline static REBACT *VAL_RELATIVE(const RELVAL *v) {
    assert(IS_RELATIVE(v));
    return ACT(v->extra.binding);
}


// When you have a RELVAL* (e.g. from a REBARR) that you "know" to be specific,
// the KNOWN macro can be used for that.  Checks to make sure in debug build.
//
// Use for: "invalid conversion from 'Reb_Value*' to 'Reb_Specific_Value*'"

inline static const REBVAL *const_KNOWN(const RELVAL *v) {
    assert(IS_END(v) or IS_SPECIFIC(v)); // END for KNOWN(ARR_HEAD()), etc.
    return cast(const REBVAL*, v);
}

inline static REBVAL *KNOWN(RELVAL *v) {
    assert(IS_END(v) or IS_SPECIFIC(v)); // END for KNOWN(ARR_HEAD()), etc.
    return cast(REBVAL*, v);
}

inline static const RELVAL *const_REL(const REBVAL *v) {
    return cast(const RELVAL*, v); // cast w/input restricted to REBVAL
}

inline static RELVAL *REL(REBVAL *v) {
    return cast(RELVAL*, v); // cast w/input restricted to REBVAL
}


//=////////////////////////////////////////////////////////////////////////=//
//
//  NULLED CELLS (*internal* form of Rebol NULL)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Rebol's null is a transient evaluation product (e.g. result of `do []`).
// It is also a signal for "soft failure", e.g. `find [a b] 'c` is null,
// hence they are conditionally false.  But null isn't an "ANY-VALUE!", and
// can't be stored in BLOCK!s that are seen by the user--nor can it be
// assigned to variables.
//
// The libRebol API takes advantage of this by actually using C's concept of
// a null pointer to directly represent the optional state.  By promising this
// is the case, clients of the API can write `if (value)` or `if (!value)`
// and be sure that there's not some nonzero address of a "null-valued cell"
// they have to make an `isRebolNull()` API call on.
//
// But that's the API.  Internal to Rebol, cells are the currency used, and
// if they are to represent an "optional" value, there must be a special
// bit pattern used to mark them as not containing any value at all.  These
// are called "nulled cells" and marked by means of their VAL_TYPE(), but they
// use REB_MAX--because that is one past the range of valid REB_XXX values
// in the enumeration created for the actual types.
//
// !!! Not using REB_0 for this has a historical reason, in trying to find
// bugs and pin down invariants in R3-Alpha, a zero bit pattern could happen
// more commonly on accident.  So 0 was "reserved" for uses that wouldn't
// come up in common practice.
//

#define REB_MAX_NULLED \
    REB_MAX // there is no NULL! datatype, use REB_MAX

#define NULLED_CELL \
    c_cast(const REBVAL*, &PG_Nulled_Cell[0])

#define IS_NULLED(v) \
    (VAL_TYPE(v) == REB_MAX_NULLED)

#ifdef NDEBUG
    inline static REBVAL *Init_Nulled(RELVAL *out) {
        RESET_VAL_CELL(out, REB_MAX_NULLED, VALUE_FLAG_FALSEY);
        return KNOWN(out);
    }
#else
    inline static REBVAL *Init_Nulled_Debug(
        RELVAL *out, const char *file, int line
    ){
        RESET_VAL_CELL_Debug(
            out,
            REB_MAX_NULLED,
            VALUE_FLAG_FALSEY,
            file,
            line
        );
        return KNOWN(out);
    }

    #define Init_Nulled(out) \
        Init_Nulled_Debug((out), __FILE__, __LINE__)
#endif

// !!! A theory was that the "evaluated" flag would help a function that took
// both <opt> and <end>, which are converted to nulls, distinguish what kind
// of null it is.  This may or may not be a good idea, but unevaluating it
// here just to make a note of the concept, and tag it via the callsites.
//
#define Init_Endish_Nulled(v) \
    RESET_VAL_CELL((v), REB_MAX_NULLED, \
        VALUE_FLAG_FALSEY | VALUE_FLAG_UNEVALUATED)

// To help ensure full nulled cells don't leak to the API, the variadic
// interface only accepts nullptr.  Any internal code with a REBVAL* that may
// be a "nulled cell" must translate any such cells to nullptr.
//
inline static const REBVAL *NULLIZE(const REBVAL *cell)
  { return VAL_TYPE(cell) == REB_MAX_NULLED ? nullptr : cell; }


//=////////////////////////////////////////////////////////////////////////=//
//
//  VOID!
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Voids are the the result given by PROCEDURE calls, and unlike NULL it *is*
// a value...however a somewhat unfriendly one.  While NULLs are falsey, voids
// are *neither* truthy nor falsey, but like NULL they can't be casually
// assigned via a SET-WORD!, SET-PATH!, or SET.  Though a void can be put in
// an array (a NULL can't) if the evaluator comes across a void cell in an
// array, it will trigger an error.
//
// Voids also come into play in what is known as "voidification" of NULLs.
// Loops wish to reserve NULL as the return result if there is a BREAK, and
// conditionals like IF and SWITCH want to reserve NULL to mean there was no
// branch taken.  So when branches or loop bodies produce null, they need
// to be converted to some ANY-VALUE!.  (Or raise an error, but raising an
// error would disllow `if true [if false [...]]`, which seems bad!)
//
// Early on in Ren-C this was done by converting NULL to BLANK!, a process
// called "blankification".  But blanks are "friendly"...which meant that
// `either condition [a] [b]` mismatched `if condition [a] else [b]` in a
// way that could be hard to diagnose, since the truthy branch in the IF case
// could silently convert nulls to blank.  Because voids are "unfriendly" and
// rarer, auto-voidifying nulls is a lesser evil than auto-blankifying them.
//
// The console doesn't print anything for void evaluation results by default,
// so that routines like HELP won't have additional output than what they
// print out.
//

#define VOID_VALUE \
    c_cast(const REBVAL*, &PG_Void_Value[0])

#ifdef NDEBUG
    inline static REBVAL *Init_Void(RELVAL *out) {
        RESET_VAL_CELL(out, REB_VOID, 0);
        return KNOWN(out);
    }
#else
    inline static REBVAL *Init_Void_Debug(
        RELVAL *out, const char *file, int line
    ){
        RESET_VAL_CELL_Debug(out, REB_VOID, 0, file, line);
        return KNOWN(out);
    }

    #define Init_Void(out) \
        Init_Void_Debug((out), __FILE__, __LINE__)
#endif

inline static REBVAL *Voidify_If_Nulled(REBVAL *cell) {
    if (IS_NULLED(cell))
        Init_Void(cell);
    return cell;
}


//=////////////////////////////////////////////////////////////////////////=//
//
//  BAR! and LIT-BAR!
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The "expression barrier" is denoted by a lone vertical bar `|`.  It
// has the special property that literals used directly will be rejected
// as a source for argument fulfillment.  BAR! that comes from evaluations
// can be passed as a parameter, however:
//
//     append [a b c] | [d e f] print "Hello"   ;-- will cause an error
//     append [a b c] [d e f] | print "Hello"   ;-- is legal
//     append [a b c] first [|]                 ;-- is legal
//     append [a b c] '|                        ;-- is legal
//

#define BAR_VALUE \
    c_cast(const REBVAL*, &PG_Bar_Value[0])

#ifdef NDEBUG
    inline static REBVAL *Init_Bar(RELVAL *out) {
        RESET_VAL_CELL(out, REB_BAR, 0);
        return KNOWN(out);
    }

    inline static REBVAL *Init_Lit_Bar(RELVAL *out) {
        RESET_VAL_CELL(out, REB_LIT_BAR, 0);
        return KNOWN(out);
    }
#else
    inline static REBVAL *Init_Bar_Debug(
        RELVAL *out, const char *file, int line
    ){
        RESET_VAL_CELL_Debug(out, REB_BAR, 0, file, line);
        return KNOWN(out);
    }

    #define Init_Bar(out) \
        Init_Bar_Debug((out), __FILE__, __LINE__)

    inline static REBVAL *Init_Lit_Bar_Debug(
        RELVAL *out, const char *file, int line
    ){
        RESET_VAL_CELL_Debug(out, REB_LIT_BAR, 0, file, line);
        return KNOWN(out);
    }

    #define Init_Lit_Bar(out) \
        Init_Lit_Bar_Debug((out), __FILE__, __LINE__)
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  BLANK!
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Blank values are a kind of "reified" null/void, and you can convert
// between them using TRY and OPT:
//
//     >> try ()
//     == _
//
//     >> opt _
//     ;-- no result
//
// Like null, they are considered to be false--like the LOGIC! #[false] value.
// Only these three things are conditionally false in Rebol, and testing for
// conditional truth and falsehood is frequent.  Hence in addition to its
// type, BLANK! also carries a header bit that can be checked for conditional
// falsehood, to save on needing to separately test the type.
//
// In the debug build, it is possible to make an "unreadable" blank.  This
// will behave neutrally as far as the garbage collector is concerned, so
// it can be used as a placeholder for a value that will be filled in at
// some later time--spanning an evaluation.  But if the special IS_UNREADABLE
// checks are not used, it will not respond to IS_BLANK() and will also
// refuse VAL_TYPE() checks.  This is useful anytime a placeholder is needed
// in a slot temporarily where the code knows it's supposed to come back and
// fill in the correct thing later...where the asserts serve as a reminder
// if that fill in never happens.
//

#define BLANK_VALUE \
    c_cast(const REBVAL*, &PG_Blank_Value[0])

#define Init_Blank(v) \
    RESET_VAL_CELL((v), REB_BLANK, VALUE_FLAG_FALSEY)

#ifdef DEBUG_UNREADABLE_BLANKS
    #define Init_Unreadable_Blank(v) \
        RESET_VAL_CELL((v), REB_BLANK, \
            VALUE_FLAG_FALSEY | TRASH_FLAG_UNREADABLE_IF_DEBUG)

    inline static REBOOL IS_BLANK_RAW(const RELVAL *v) {
        return VAL_TYPE_RAW(v) == REB_BLANK;
    }

    inline static REBOOL IS_UNREADABLE_DEBUG(const RELVAL *v) {
        if (VAL_TYPE_RAW(v) != REB_BLANK)
            return FALSE;
        return did (v->header.bits & TRASH_FLAG_UNREADABLE_IF_DEBUG);
    }

    // "Sinking" a value is like trashing it in the debug build at the moment
    // of knowing that it will ultimately be overwritten.  This avoids
    // any accidental usage of the target cell's contents before the overwrite
    // winds up happening.
    //
    // It's slightly different than "trashing", because if the node was valid
    // before, then it would have been safe for the GC to visit.  So this
    // doesn't break that invariant...if the node was invalid it stays
    // invalid, but if it was valid it is turned into an unreadable blank,
    // which overwrites all the cell fields (with tracking info) and will
    // trigger errors through VAL_TYPE() if it's used.
    //
    inline static REBVAL *Sink_Debug(
        RELVAL *v

      #if defined(DEBUG_TRACK_CELLS) || defined(DEBUG_CELL_WRITABILITY)
      , const char *file
      , int line
      #endif
    ){
        ASSERT_CELL_WRITABLE_EVIL_MACRO(v, file, line);

        if (IS_TRASH_DEBUG(v)) {
            // already trash, don't need to mess with the header
        }
        else {
          #ifdef DEBUG_CELL_WRITABILITY
            RESET_VAL_HEADER_EXTRA_Core(
                v,
                REB_BLANK,
                VALUE_FLAG_FALSEY | TRASH_FLAG_UNREADABLE_IF_DEBUG,
                file,
                line
            );
          #else
            RESET_VAL_HEADER_EXTRA(
                v,
                REB_BLANK,
                VALUE_FLAG_FALSEY | TRASH_FLAG_UNREADABLE_IF_DEBUG
            );
          #endif
        }

        TRACK_CELL_IF_DEBUG(v, file, line);

        return cast(REBVAL*, v); // used by SINK, but not TRASH_CELL_IF_DEBUG
    }

    #if defined(DEBUG_TRACK_CELLS) || defined(DEBUG_CELL_WRITABILITY)
        #define SINK(v) \
            Sink_Debug((v), __FILE__, __LINE__)
    #else
        #define SINK(v) \
            Sink_Debug(v)
    #endif

    #define ASSERT_UNREADABLE_IF_DEBUG(v) \
        assert(IS_UNREADABLE_DEBUG(v))

    #define ASSERT_READABLE_IF_DEBUG(v) \
        assert(not IS_UNREADABLE_DEBUG(v))
#else
    #define Init_Unreadable_Blank(v) \
        Init_Blank(v)

    #define IS_BLANK_RAW(v) \
        IS_BLANK(v)

    #define ASSERT_UNREADABLE_IF_DEBUG(v) \
        assert(IS_BLANK(v)) // would have to be a blank even if not unreadable

    #define ASSERT_READABLE_IF_DEBUG(v) \
        NOOP

    #define SINK(v) \
        cast(REBVAL*, (v))
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  LOGIC!
//
//=////////////////////////////////////////////////////////////////////////=//
//
// A logic can be either true or false.  For purposes of optimization, logical
// falsehood is indicated by one of the value option bits in the header--as
// opposed to in the value payload.  This means it can be tested quickly, and
// that a single check can test for both BLANK! and logic false.
//
// Conditional truth and falsehood allows an interpretation where a BLANK!
// is a "falsey" value as well.
//

#define FALSE_VALUE \
    c_cast(const REBVAL*, &PG_False_Value[0])

#define TRUE_VALUE \
    c_cast(const REBVAL*, &PG_True_Value[0])

inline static REBOOL IS_TRUTHY(const RELVAL *v) {
    if (GET_VAL_FLAG(v, VALUE_FLAG_FALSEY))
        return false;
    if (IS_VOID(v))
        fail (Error_Void_Conditional_Raw());
    return true;
}

#define IS_FALSEY(v) \
    (not IS_TRUTHY(v))


#ifdef NDEBUG
    inline static REBVAL *Init_Logic(RELVAL *out, REBOOL b) {
        RESET_VAL_CELL(out, REB_LOGIC, b ? 0 : VALUE_FLAG_FALSEY);
        return KNOWN(out);
    }
#else
    inline static REBVAL *Init_Logic_Debug(
        RELVAL *out, REBOOL b, const char *file, int line
    ){
        RESET_VAL_CELL_Debug(
            out,
            REB_LOGIC,
            b ? 0 : VALUE_FLAG_FALSEY,
            file,
            line
        );
        return KNOWN(out);
    }

    #define Init_Logic(out,b) \
        Init_Logic_Debug((out), (b), __FILE__, __LINE__)
#endif


// Although a BLOCK! value is true, some constructs are safer by not allowing
// literal blocks.  e.g. `if [x] [print "this is not safe"]`.  The evaluated
// bit can let these instances be distinguished.  Note that making *all*
// evaluations safe would be limiting, e.g. `foo: any [false-thing []]`...
// So ANY and ALL use IS_TRUTHY() directly
//
inline static REBOOL IS_CONDITIONAL_TRUE(const REBVAL *v) {
    if (GET_VAL_FLAG(v, VALUE_FLAG_FALSEY))
        return false;
    if (IS_VOID(v))
        fail (Error_Void_Conditional_Raw());
    if (IS_BLOCK(v) and GET_VAL_FLAG(v, VALUE_FLAG_UNEVALUATED))
        fail (Error_Block_Conditional_Raw(v));
    return true;
}

#define IS_CONDITIONAL_FALSE(v) \
    (not IS_CONDITIONAL_TRUE(v))

inline static REBOOL VAL_LOGIC(const RELVAL *v) {
    assert(IS_LOGIC(v));
    return NOT_VAL_FLAG((v), VALUE_FLAG_FALSEY);
}


//=////////////////////////////////////////////////////////////////////////=//
//
//  DATATYPE!
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Note: R3-Alpha's notion of a datatype has not been revisited very much in
// Ren-C.  The unimplemented UTYPE! user-defined type concept was removed
// for simplification, pending a broader review of what was needed.
//
// %words.r is arranged so that symbols for types are at the start
// Although REB_0 is 0 and the 0 REBCNT used for symbol IDs is reserved
// for "no symbol"...this is okay, because void is not a value type and
// should not have a symbol.
//
// !!! Consider the naming once all legacy TYPE? calls have been converted
// to TYPE-OF.  TYPE! may be a better name, though possibly KIND! would be
// better if user types suggest that TYPE-OF can potentially return some
// kind of context (might TYPE! be an ANY-CONTEXT!, with properties like
// MIN-VALUE and MAX-VALUE, for instance).
//

#define VAL_TYPE_KIND(v) \
    ((v)->payload.datatype.kind)

#define VAL_TYPE_SPEC(v) \
    ((v)->payload.datatype.spec)


//=////////////////////////////////////////////////////////////////////////=//
//
//  CHAR!
//
//=////////////////////////////////////////////////////////////////////////=//

#define MAX_CHAR 0xffff

#define VAL_CHAR(v) \
    ((v)->payload.character)

inline static REBVAL *Init_Char(RELVAL *out, REBUNI uni) {
    RESET_VAL_HEADER(out, REB_CHAR);
    VAL_CHAR(out) = uni;
    return cast(REBVAL*, out);
}

#define SPACE_VALUE \
    Root_Space_Char

#define NEWLINE_VALUE \
    Root_Newline_Char


//=////////////////////////////////////////////////////////////////////////=//
//
//  INTEGER!
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Integers in Rebol were standardized to use a compiler-provided 64-bit
// value.  This was formally added to the spec in C99, but many compilers
// supported it before that.
//
// !!! 64-bit extensions were added by the "rebolsource" fork, with much of
// the code still written to operate on 32-bit values.  Since the standard
// unit of indexing and block length counts remains 32-bit in that 64-bit
// build at the moment, many lingering references were left that operated
// on 32-bit values.  To make this clearer, the macros have been renamed
// to indicate which kind of integer they retrieve.  However, there should
// be a general review for reasoning, and error handling + overflow logic
// for these cases.
//

#if defined(NDEBUG) || !defined(CPLUSPLUS_11) 
    #define VAL_INT64(v) \
        ((v)->payload.integer)
#else
    // allows an assert, but also lvalue: `VAL_INT64(v) = xxx`
    //
    inline static REBI64 & VAL_INT64(RELVAL *v) { // C++ reference type
        assert(IS_INTEGER(v));
        return v->payload.integer;
    }
    inline static REBI64 VAL_INT64(const RELVAL *v) {
        assert(IS_INTEGER(v));
        return v->payload.integer;
    }
#endif

inline static REBVAL *Init_Integer(RELVAL *out, REBI64 i64) {
    RESET_VAL_HEADER(out, REB_INTEGER);
    out->payload.integer = i64;
    UNUSED(out->extra.binding);
    return cast(REBVAL*, out);
}

#define VAL_INT32(v) \
    cast(REBINT, VAL_INT64(v))

#define VAL_UNT32(v) \
    cast(REBCNT, VAL_INT64(v))


//=////////////////////////////////////////////////////////////////////////=//
//
//  DECIMAL! and PERCENT!
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Implementation-wise, the decimal type is a `double`-precision floating
// point number in C (typically 64-bit).  The percent type uses the same
// payload, and is currently extracted with VAL_DECIMAL() as well.
//
// !!! Calling a floating point type "decimal" appears based on Rebol's
// original desire to use familiar words and avoid jargon.  It has however
// drawn criticism from those who don't think it correctly conveys floating
// point behavior, expecting something else.  Red has renamed the type
// FLOAT! which may be a good idea.
//

#if defined(NDEBUG) || !defined(CPLUSPLUS_11)
    #define VAL_DECIMAL(v) \
        ((v)->payload.decimal)
#else
    // allows an assert, but also lvalue: `VAL_DECIMAL(v) = xxx`
    //
    inline static REBDEC & VAL_DECIMAL(RELVAL *v) { // C++ reference type
        assert(IS_DECIMAL(v) or IS_PERCENT(v));
        return v->payload.decimal;
    }
    inline static REBDEC VAL_DECIMAL(const RELVAL *v) {
        assert(IS_DECIMAL(v) or IS_PERCENT(v));
        return v->payload.decimal;
    }
#endif

inline static REBVAL *Init_Decimal(RELVAL *out, REBDEC d) {
    RESET_VAL_HEADER(out, REB_DECIMAL);
    out->payload.decimal = d;
    return cast(REBVAL*, out);
}

inline static REBVAL *Init_Percent(RELVAL *out, REBDEC d) {
    RESET_VAL_HEADER(out, REB_PERCENT);
    out->payload.decimal = d;
    return cast(REBVAL*, out);
}


//=////////////////////////////////////////////////////////////////////////=//
//
//  MONEY!
//
//=////////////////////////////////////////////////////////////////////////=//
//
// R3-Alpha's MONEY! type is "unitless" currency, such that $10/$10 = $1
// (and not 1).  This is because the feature in Rebol2 of being able to
// store the ISO 4217 code (~15 bits) was not included:
//
// https://en.wikipedia.org/wiki/ISO_4217
//
// According to @Ladislav:
//
// "The money datatype is neither a bignum, nor a fixpoint arithmetic.
//  It actually is unnormalized decimal floating point."
//
// !!! The naming of "deci" used by MONEY! as "decimal" is a confusing overlap
// with DECIMAL!, although that name may be changing also.
//

inline static deci VAL_MONEY_AMOUNT(const RELVAL *v) {
    deci amount;
    amount.m0 = v->extra.m0;
    amount.m1 = v->payload.money.m1;
    amount.m2 = v->payload.money.m2;
    amount.s = v->payload.money.s;
    amount.e = v->payload.money.e;
    return amount;
}

inline static REBVAL *Init_Money(RELVAL *out, deci amount) {
    RESET_VAL_HEADER(out, REB_MONEY);
    out->extra.m0 = amount.m0;
    out->payload.money.m1 = amount.m1;
    out->payload.money.m2 = amount.m2;
    out->payload.money.s = amount.s;
    out->payload.money.e = amount.e;
    return cast(REBVAL*, out);
}


//=////////////////////////////////////////////////////////////////////////=//
//
//  TUPLE!
//
//=////////////////////////////////////////////////////////////////////////=//
//
// TUPLE! is a Rebol2/R3-Alpha concept to fit up to 7 byte-sized integers
// directly into a value payload without needing to make a series allocation.
// At source level they would be numbers separated by dots, like `1.2.3.4.5`.
// This was mainly applied for IP addresses and RGB/RGBA constants, and
// considered to be a "lightweight"...it would allow PICK and POKE like a
// series, but did not behave like one due to not having a position.
//
// !!! Ren-C challenges the value of the TUPLE! type as defined.  Color
// literals are often hexadecimal (where BINARY! would do) and IPv6 addresses
// have a different notation.  It may be that `.` could be used for a more
// generalized partner to PATH!, where `a.b.1` would be like a/b/1
//

#define MAX_TUPLE \
    ((sizeof(uint32_t) * 2) - 1) // for same properties on 64-bit and 32-bit

#define VAL_TUPLE(v) \
    ((v)->payload.tuple.tuple + 1)

#define VAL_TUPLE_LEN(v) \
    ((v)->payload.tuple.tuple[0])

#define VAL_TUPLE_DATA(v) \
    ((v)->payload.tuple.tuple)

inline static REBVAL *SET_TUPLE(RELVAL *out, const void *data) {
    RESET_VAL_HEADER(out, REB_TUPLE);
    memcpy(VAL_TUPLE_DATA(out), data, sizeof(VAL_TUPLE_DATA(out)));
    return cast(REBVAL*, out);
}



//=////////////////////////////////////////////////////////////////////////=//
//
//  EVENT!
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Rebol's events are used for the GUI and for network and I/O.  They are
// essentially just a union of some structures which are packed so they can
// fit into a REBVAL's payload size.
//
// The available event models are:
//
// * EVM_PORT
// * EVM_OBJECT
// * EVM_DEVICE
// * EVM_CALLBACK
// * EVM_GUI
//

#define VAL_EVENT_TYPE(v) \
    ((v)->payload.event.type)

#define VAL_EVENT_FLAGS(v) \
    ((v)->payload.event.flags)

#define VAL_EVENT_WIN(v) \
    ((v)->payload.event.win)

#define VAL_EVENT_MODEL(v) \
    ((v)->payload.event.model)

#define VAL_EVENT_DATA(v) \
    ((v)->payload.event.data)

#define VAL_EVENT_TIME(v) \
    ((v)->payload.event.time)

#define VAL_EVENT_REQ(v) \
    ((v)->extra.eventee.req)

#define VAL_EVENT_SER(v) \
    ((v)->extra.eventee.ser)

#define IS_EVENT_MODEL(v,f) \
    (VAL_EVENT_MODEL(v) == (f))

inline static void SET_EVENT_INFO(
    RELVAL *val,
    uint8_t type,
    uint8_t flags,
    uint8_t win
){
    VAL_EVENT_TYPE(val) = type;
    VAL_EVENT_FLAGS(val) = flags;
    VAL_EVENT_WIN(val) = win;
}

// Position event data

#define VAL_EVENT_X(v) \
    cast(REBINT, cast(short, VAL_EVENT_DATA(v) & 0xffff))

#define VAL_EVENT_Y(v) \
    cast(REBINT, cast(short, (VAL_EVENT_DATA(v) >> 16) & 0xffff))

#define VAL_EVENT_XY(v) \
    (VAL_EVENT_DATA(v))

inline static void SET_EVENT_XY(RELVAL *v, REBINT x, REBINT y) {
    //
    // !!! "conversion to u32 from REBINT may change the sign of the result"
    // Hence cast.  Not clear what the intent is.
    //
    VAL_EVENT_DATA(v) = cast(uint32_t, ((y << 16) | (x & 0xffff)));
}

// Key event data

#define VAL_EVENT_KEY(v) \
    (VAL_EVENT_DATA(v) & 0xffff)

#define VAL_EVENT_KCODE(v) \
    ((VAL_EVENT_DATA(v) >> 16) & 0xffff)

inline static void SET_EVENT_KEY(RELVAL *v, REBCNT k, REBCNT c) {
    VAL_EVENT_DATA(v) = ((c << 16) + k);
}


//=////////////////////////////////////////////////////////////////////////=//
//
//  IMAGE!
//
//=////////////////////////////////////////////////////////////////////////=//
//
// !!! Ren-C's primary goals are to research and pin down fundamentals, where
// things like IMAGE! would be an extension through a user-defined type
// vs. being in the core.  The R3-Alpha code has been kept compiling here
// due to its usage in R3-GUI.
//

// QUAD=(Red, Green, Blue, Alpha)

#define QUAD_LEN(s) \
    SER_LEN(s)

#define QUAD_HEAD(s) \
    SER_DATA_RAW(s)

#define QUAD_SKIP(s,n) \
    (QUAD_HEAD(s) + ((n) * 4))

#define QUAD_TAIL(s) \
    (QUAD_HEAD(s) + (QUAD_LEN(s) * 4))

#define IMG_WIDE(s) \
    (MISC(s).area.wide)

#define IMG_HIGH(s) \
    (MISC(s).area.high)

#define IMG_DATA(s) \
    SER_DATA_RAW(s)

#define VAL_IMAGE_HEAD(v) \
    QUAD_HEAD(VAL_SERIES(v))

#define VAL_IMAGE_TAIL(v) \
    QUAD_SKIP(VAL_SERIES(v), VAL_LEN_HEAD(v))

#define VAL_IMAGE_DATA(v) \
    QUAD_SKIP(VAL_SERIES(v), VAL_INDEX(v))

#define VAL_IMAGE_BITS(v) \
    cast(REBCNT*, VAL_IMAGE_HEAD(v))

#define VAL_IMAGE_WIDE(v) \
    (IMG_WIDE(VAL_SERIES(v)))

#define VAL_IMAGE_HIGH(v) \
    (IMG_HIGH(VAL_SERIES(v)))

#define VAL_IMAGE_LEN(v) \
    VAL_LEN_AT(v)

#define Init_Image(out,s) \
    Init_Any_Series((out), REB_IMAGE, (s));

//tuple to image! pixel order bytes
#define TO_PIXEL_TUPLE(t) \
    TO_PIXEL_COLOR(VAL_TUPLE(t)[0], VAL_TUPLE(t)[1], VAL_TUPLE(t)[2], \
        VAL_TUPLE_LEN(t) > 3 ? VAL_TUPLE(t)[3] : 0xff)

//tuple to RGBA bytes
#define TO_COLOR_TUPLE(t) \
    TO_RGBA_COLOR(VAL_TUPLE(t)[0], VAL_TUPLE(t)[1], VAL_TUPLE(t)[2], \
        VAL_TUPLE_LEN(t) > 3 ? VAL_TUPLE(t)[3] : 0xff)


//=////////////////////////////////////////////////////////////////////////=//
//
//  GOB! Graphic Object
//
//=////////////////////////////////////////////////////////////////////////=//
//
// !!! The GOB! is a datatype specific to R3-View.  Its data is a small
// fixed-size object.  It is linked together by series containing more
// GOBs and values, and participates in the garbage collection process.
//
// The monolithic structure of Rebol had made it desirable to take advantage
// of the memory pooling to quickly allocate, free, and garbage collect
// these.  With GOB! being moved to an extension, it is not likely that it
// would hook the memory pools directly.
//

#if defined(NDEBUG) || !defined(CPLUSPLUS_11)
    #define VAL_GOB(v) \
        (v)->payload.gob.gob

    #define VAL_GOB_INDEX(v) \
        (v)->payload.gob.index
#else
    inline static REBGOB* const &VAL_GOB(const RELVAL *v) {
        assert(IS_GOB(v));
        return v->payload.gob.gob;
    }

    inline static REBCNT const &VAL_GOB_INDEX(const RELVAL *v) {
        assert(IS_GOB(v));
        return v->payload.gob.index;
    }

    inline static REBGOB* &VAL_GOB(RELVAL *v) {
        assert(IS_GOB(v));
        return v->payload.gob.gob;
    }

    inline static REBCNT &VAL_GOB_INDEX(RELVAL *v) {
        assert(IS_GOB(v));
        return v->payload.gob.index;
    }
#endif

inline static void SET_GOB(RELVAL *v, REBGOB *g) {
    RESET_VAL_HEADER(v, REB_GOB);
    VAL_GOB(v) = g;
    VAL_GOB_INDEX(v) = 0;
}


//=////////////////////////////////////////////////////////////////////////=//
//
//  BINDING
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Some value types use their `->extra` field in order to store a pointer to
// a REBNOD which constitutes their notion of "binding".
//
// This can either be null (a.k.a. UNBOUND), or to a function's paramlist
// (indicates a relative binding), or to a context's varlist (which indicates
// a specific binding.)
//
// NOTE: Instead of using null for UNBOUND, a special global REBSER struct was
// experimented with.  It was at a location in memory known at compile time,
// and it had its ->header and ->info bits set in such a way as to avoid the
// need for some conditional checks.  e.g. instead of writing:
//
//     if (binding and binding->header.bits & NODE_FLAG_MANAGED) {...}
//
// The special UNBOUND node set some bits, such as to pretend to be managed:
//
//     if (binding->header.bits & NODE_FLAG_MANAGED) {...} // incl. UNBOUND
//
// Question was whether avoiding the branching involved from the extra test
// for null would be worth it for a consistent ability to dereference.  At
// least on x86/x64, the answer was: No.  It was maybe even a little slower.
// Testing for null pointers the processor has in its hand is very common and
// seemed to outweigh the need to dereference all the time.  The increased
// clarity of having unbound be nullptr is also in its benefit.
//
// NOTE: The ordering of %types.r is chosen specially so that all bindable
// types are at lower values than the unbindable types.
//

#define SPECIFIED \
    cast(REBSPC*, 0) // cast() doesn't like nullptr, fix

#define UNBOUND \
   cast(REBNOD*, 0) // cast() doesn't like nullptr, fix

inline static REBNOD *VAL_BINDING(const RELVAL *v) {
    assert(Is_Bindable(v));
    return v->extra.binding;
}

inline static void INIT_BINDING(RELVAL *v, void *p) {
    assert(Is_Bindable(v)); // works on partially formed values

    REBNOD *binding = cast(REBNOD*, p);
    v->extra.binding = binding;

  #if !defined(NDEBUG)
    if (not binding)
        return; // e.g. UNBOUND

    assert(not (binding->header.bits & NODE_FLAG_CELL)); // not currently used

    if (binding->header.bits & NODE_FLAG_MANAGED) {
        assert(
            binding->header.bits & ARRAY_FLAG_VARLIST // specific
            or binding->header.bits & ARRAY_FLAG_PARAMLIST // relative
            or (
                IS_VARARGS(v)
                and not (SER(binding)->header.bits & SERIES_FLAG_HAS_DYNAMIC)
            ) // varargs from MAKE VARARGS! [...], else is a varlist
        );
    }
    else {
        // Can only store unmanaged pointers in stack cells (and only if the
        // lifetime of the stack entry is guaranteed to outlive the binding)
        //
        assert(CTX(p));
        if (v->header.bits & NODE_FLAG_TRANSIENT) {
            // let anything go... for now.
            // SERIES_FLAG_STACK might not be set yet due to construction
            // constraints, see Make_Context_For_Action_Int_Partials()
        }
        else {
            assert(v->header.bits & CELL_FLAG_STACK);
            assert(binding->header.bits & SERIES_FLAG_STACK);
        }
    }
  #endif
}

inline static void Move_Value_Header(RELVAL *out, const RELVAL *v)
{
    assert(out != v); // usually a sign of a mistake; not worth supporting
    assert(NOT_END(v)); // SET_END() is the only way to write an end

    ASSERT_CELL_WRITABLE_EVIL_MACRO(out, __FILE__, __LINE__);

    out->header.bits &= CELL_MASK_PERSIST;
    out->header.bits |= v->header.bits & CELL_MASK_COPY;

  #ifdef DEBUG_TRACK_EXTEND_CELLS
    out->track = v->track;
    out->tick = v->tick; // initialization tick
    out->touch = v->touch; // arbitrary debugging use via TOUCH_CELL
  #endif
}


// If the cell we're writing into is a stack cell, there's a chance that
// management/reification of the binding can be avoided.
//
inline static void INIT_BINDING_MAY_MANAGE(RELVAL *out, REBNOD* binding) {
    if (not binding) {
        out->extra.binding = nullptr; // unbound
        return;
    }
    if (GET_SER_FLAG(binding, NODE_FLAG_MANAGED)) {
        out->extra.binding = binding; // managed is safe for any `out`
        return;
    }
    if (out->header.bits & NODE_FLAG_TRANSIENT) {
        out->extra.binding = binding; // can't be passed between frame levels
        return;
    }

    assert(GET_SER_FLAG(binding, SERIES_FLAG_STACK));
 
    REBFRM *f = FRM(LINK(binding).keysource);
    assert(IS_END(f->param)); // cannot manage frame varlist in mid fulfill!
    UNUSED(f); // !!! not actually used yet, coming soon

    if (out->header.bits & NODE_FLAG_STACK) {
        //
        // If the cell we're writing to is a stack cell, there's a chance
        // that management/reification of the binding can be avoided.
        //
        REBCNT bind_depth = 1; // !!! need to find v's binding stack level
        REBCNT out_depth;
        if (not (out->header.bits & CELL_FLAG_STACK))
            out_depth = 0;
        else
            out_depth = 1; // !!! need to find out's stack level

        REBOOL smarts_enabled = false; 
        if (smarts_enabled and out_depth >= bind_depth)
            return; // binding will outlive `out`, don't manage

        // no luck...`out` might outlive the binding, must manage
    }

    binding->header.bits |= NODE_FLAG_MANAGED; // burdens the GC, now...
    out->extra.binding = binding;
}


// !!! Because you cannot assign REBVALs to one another (e.g. `*dest = *src`)
// a function is used.  The reason that a function is used is because this
// gives more flexibility in decisions based on the destination cell regarding
// whether it is necessary to reify information in the source cell.
//
// That advanced purpose has not yet been implemented, because it requires
// being able to "sniff" a cell for its lifetime.  For now it only preserves
// the CELL_FLAG_STACK bit, without actually doing anything with it.
//
// Interface designed to line up with Derelativize()
//
inline static REBVAL *Move_Value(RELVAL *out, const REBVAL *v)
{
    Move_Value_Header(out, v);

    if (Not_Bindable(v))
        out->extra = v->extra; // extra isn't a binding (INTEGER! MONEY!...)
    else
        INIT_BINDING_MAY_MANAGE(out, v->extra.binding);

    out->payload = v->payload; // payloads cannot hold references to stackvars
    return KNOWN(out);
}


// When doing something like a COPY of an OBJECT!, the var cells have to be
// handled specially, e.g. by preserving VALUE_FLAG_ENFIXED.
//
// !!! What about other non-copyable properties like CELL_FLAG_PROTECTED?
//
inline static REBVAL *Move_Var(RELVAL *out, const REBVAL *v)
{
    assert(not (out->header.bits & CELL_FLAG_STACK));

    // This special kind of copy can only be done into another object's
    // variable slot. (Since the source may be a FRAME!, v *might* be stack
    // but it should never be relative.  If it's stack, we have to go through
    // the whole potential reification process...double-set header for now.)

    Move_Value(out, v);
    out->header.bits |= (
        v->header.bits & (VALUE_FLAG_ENFIXED | ARG_MARKED_CHECKED)
    );
    return KNOWN(out);
}


// Generally speaking, you cannot take a RELVAL from one cell and copy it
// blindly into another...it needs to be Derelativize()'d.  This routine is
// for the rare cases where it's legal, e.g. shuffling a cell from one place
// in an array to another cell in the same array.
//
inline static void Blit_Cell(RELVAL *out, const RELVAL *v)
{
    assert(out != v); // usually a sign of a mistake; not worth supporting
    assert(NOT_END(v));

    ASSERT_CELL_WRITABLE_EVIL_MACRO(out, __FILE__, __LINE__);

    // Examine just the cell's preparation bits.  Are they identical?  If so,
    // we are not losing any information by blindly copying the header in
    // the release build.
    //
    assert(
        (out->header.bits & CELL_MASK_PERSIST)
        == (v->header.bits & CELL_MASK_PERSIST)
    );

    out->header = v->header;
    out->payload = v->payload;
    out->extra = v->extra;
}


//
// Rather than allow a REBVAL to be declared plainly as a local variable in
// a C function, this macro provides a generic "constructor-like" hook.
// See CELL_FLAG_STACK for the experimental motivation.  However, even if
// this were merely a synonym for a plain REBVAL declaration in the release
// build, it provides a useful generic hook into the point of declaration
// of a stack value.
//
// Note: because this will run instructions, a routine should avoid doing a
// DECLARE_LOCAL inside of a loop.  It should be at the outermost scope of
// the function.
//
// Note: It sets NODE_FLAG_FREE, so this is a "trash" cell by default.
//
#define DECLARE_LOCAL(name) \
    REBVAL name##_pair[2]; \
    Prep_Stack_Cell(cast(REBVAL*, &name##_pair)); /* tbd: FS_TOP FRAME! */ \
    REBVAL * const name = cast(REBVAL*, &name##_pair) + 1; \
    Prep_Stack_Cell(name)
