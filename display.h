/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include "snes9x.h"

void S9xUsage (void);
char * S9xParseArgs (char **, int);
void S9xParseArgsForCheats (char **, int);
void S9xLoadConfigFiles (char **, int);
// timeout_frames == 0 means "use Settings.InitialInfoStringTimeout".
void S9xSetInfoString (const char *, uint32 timeout_frames = 0);
void S9xClearInfoString (void);
// Clamps every currently-queued info message's remaining time to at most
// max_frames (e.g. so messages don't linger for real-world minutes while
// single-stepping frames).
void S9xCapInfoStringTimeout (uint32 max_frames);
// RGBA8888 icon shown beside the InfoString (e.g. an achievement badge).
void S9xSetInfoImage (const uint8 *rgba, int width, int height);
void S9xClearInfoImage (void);

// Routines the port has to implement even if it doesn't use them

void S9xPutImage (int, int);
void S9xInitDisplay (int, char **);
void S9xDeinitDisplay (void);
void S9xTextMode (void);
void S9xGraphicsMode (void);
void S9xToggleSoundChannel (int);
bool8 S9xOpenSnapshotFile (const char *, bool8, STREAM *);
void S9xCloseSnapshotFile (STREAM);
const char * S9xStringInput (const char *);

// Routines the port has to implement if it uses command-line

void S9xExtraUsage (void);
void S9xParseArg (char **, int &, int);

// Routines the port may implement as needed

void S9xExtraDisplayUsage (void);
void S9xParseDisplayArg (char **, int &, int);
void S9xSetTitle (const char *);
void S9xInitInputDevices (void);
void S9xProcessEvents (bool8);
const char * S9xSelectFilename (const char *, const char *, const char *, const char *);

#endif
