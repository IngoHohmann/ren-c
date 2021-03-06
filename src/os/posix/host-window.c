//
//  File: %host-window.c
//  Summary: "Windowing stubs"
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
// Provides stub functions for windowing.
//
//=////////////////////////////////////////////////////////////////////////=//
//
// WARNING: The function declarations here cannot be modified without also
// modifying those found in the other OS host-lib files!  Do not even modify
// the argument names.
//

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>

#include "reb-host.h"
#include "rebol-lib.h"


//**********************************************************************
//** OSAL Library Functions ********************************************
//**********************************************************************

//
//  OS_Init_Graphics: C
//
// Initialize graphics subsystem. Store Gob_Root.
//
void OS_Init_Graphics(REBGOB *gob)
{
    UNUSED(gob);
}


//
//  OS_GUI_Metrics: C
//
// Provide info about the hosting GUI.
//
void OS_GUI_Metrics(REBOL_OS_METRICS *met)
{
    UNUSED(met);
}


//
//  OS_Show_Gob: C
//
// Notes:
//     1.    Can be called with NONE (0), Gob_Root (All), or a
//         specific gob to open, close, or refresh.
//
//     2.    A new window will be in Gob_Root/pane but will not
//         have GOBF_WINDOW set.
//
//     3.    A closed window will have no PARENT and will not be
//         in the Gob_Root/pane but will have GOBF_WINDOW set.
//
REBINT OS_Show_Gob(REBGOB *gob)
{
    UNUSED(gob);

    return 0;
}


//
//  OS_Map_Gob: C
//
// Map GOB and offset to inner or outer GOB and offset.
//
void OS_Map_Gob(REBGOB **gob, REBPAR *xy, REBOOL inner)
{
    UNUSED(gob);
    UNUSED(xy);
    UNUSED(inner);
}


//
//  OS_Size_Text: C
//
// Return the area size of the text.
//
REBINT OS_Size_Text(REBGOB *gob, REBPAR *size)
{
    UNUSED(gob);
    UNUSED(size);

    return 0;
}


//
//  OS_Offset_To_Caret: C
//
// Return the element and position for a given offset pair.
//
REBINT OS_Offset_To_Caret(REBGOB *gob, REBPAR xy, REBINT *element, REBINT *position)
{
    UNUSED(gob);
    UNUSED(xy);
    UNUSED(element);
    UNUSED(position);

    return 0;
}


//
//  OS_Caret_To_Offset: C
//
// Return the offset pair for a given element and position.
//
REBINT OS_Caret_To_Offset(REBGOB *gob, REBPAR *xy, REBINT element, REBINT position)
{
    UNUSED(gob);
    UNUSED(xy);
    UNUSED(element);
    UNUSED(position);

    return 0;
}


//
//  OS_Gob_To_Image: C
//
// Render gob into an image.
// Clip to keep render inside the image provided.
//
REBINT OS_Gob_To_Image(REBSER *image, REBGOB *gob)
{
    UNUSED(image);
    UNUSED(gob);

    return 0;
}


//
//  OS_Draw_Image: C
//
// Render DRAW dialect into an image.
// Clip to keep render inside the image provided.
//
REBINT OS_Draw_Image(REBSER *image, REBARR *block)
{
    UNUSED(image);
    UNUSED(block);

    return 0;
}


//
//  OS_Effect_Image: C
//
// Render EFFECT dialect into an image.
// Clip to keep render inside the image provided.
//
REBINT OS_Effect_Image(REBSER *image, REBARR *block)
{
    UNUSED(image);
    UNUSED(block);

    return 0;
}

//
//  OS_Cursor_Image: C
//
void OS_Cursor_Image(REBINT n, REBSER *image)
{
    UNUSED(n);
    UNUSED(image);
}
