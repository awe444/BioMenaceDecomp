/* Compatibility header for SDL port of BioMenace
 *
 * This file maps DOS/Borland-specific types and macros to modern C equivalents.
 * Include this instead of DOS-specific headers.
 *
 * Copyright (C) 2024 Nikolai Wuttke-Hohendorf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef ID_COMPAT_H
#define ID_COMPAT_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Do NOT include <stdbool.h> - the game defines its own boolean type
 * as: typedef enum {false,true} boolean;
 * C11's stdbool.h would conflict with the false/true identifiers.
 */

/* ========================================================================
 * DOS Memory Model Keywords - expand to nothing on modern compilers
 * ======================================================================== */
#define far
#define near
#define _seg
#define huge
#define _far
#define _near
#define _huge

/* Borland interrupt keyword */
#define interrupt

/* ========================================================================
 * DOS Pointer Macros - no-ops on flat memory model
 * ======================================================================== */
#define FP_SEG(p)  (0)
#define FP_OFF(p)  ((uintptr_t)(p))
#define MK_FP(s,o) ((void*)(uintptr_t)(o))

/* ========================================================================
 * memptr - was 'void _seg *' in DOS (a segment pointer)
 * Now just a regular void pointer.
 * ======================================================================== */
typedef void *memptr;

/* ========================================================================
 * Borland-specific functions
 * ======================================================================== */

/* movedata(srcseg, srcoff, dstseg, dstoff, nbytes) - just memcpy */
static inline void movedata(unsigned srcseg, unsigned srcoff,
                            unsigned dstseg, unsigned dstoff,
                            size_t n)
{
    (void)srcseg;
    (void)dstseg;
    /* In flat memory model, the offset IS the pointer */
    /* This is only used during MM_SortMem which we've replaced */
}

/* farmalloc / farfree - just malloc/free */
#define farmalloc(s) malloc(s)
#define farfree(p)   free(p)
#define farcoreleft() (1024L*1024L*64L)

/* coreleft - return large value (plenty of memory) */
#define coreleft() (1024L*1024L*64L)

/* ========================================================================
 * Borland/DOS I/O compatibility
 * ======================================================================== */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

/* Borland open flags */
#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef O_TEXT
#define O_TEXT 0
#endif

/* Borland filelength() */
static inline long filelength(int fd)
{
    long pos = lseek(fd, 0, SEEK_CUR);
    long end = lseek(fd, 0, SEEK_END);
    lseek(fd, pos, SEEK_SET);
    return end;
}

/* ========================================================================
 * Borland conio.h / bios.h stubs
 * ======================================================================== */

/* Cursor types (Borland conio.h) */
#define _NOCURSOR     0
#define _NORMALCURSOR 1
#define _SOLIDCURSOR  2

static inline void _setcursortype(int type) { (void)type; }

/* textmode/textcolor/gotoxy stubs */
#define C80 3
static inline void textmode(int mode) { (void)mode; }
static inline void textcolor(int color) { (void)color; }
static inline void gotoxy(int x, int y) { (void)x; (void)y; }
static inline void clrscr(void) {}
static inline int wherex(void) { return 1; }
static inline int wherey(void) { return 1; }
static inline void cputs(const char *s) { fputs(s, stdout); }

/* bioskey stub */
#define _KEYBRD_READY 1
#define _KEYBRD_READ  0
static inline int bioskey(int cmd) { (void)cmd; return 0; }

/* ========================================================================
 * DOS geninterrupt / port I/O - stubbed out for SDL port
 * ======================================================================== */
#define geninterrupt(n) ((void)0)

static inline void outportb(unsigned port, unsigned char val) {
    (void)port; (void)val;
}
static inline unsigned char inportb(unsigned port) {
    (void)port; return 0;
}

/* Borland pseudo-register variables - used by interrupt/BIOS calls */
/* We define them as unused locals to avoid compilation errors */
static unsigned short _AX, _BX, _CX, _DX;
static unsigned short _AH, _AL, _BH, _BL, _CH, _CL, _DH, _DL;
static unsigned short _SI, _DI, _ES, _DS;

/* ========================================================================
 * DOS process control
 * ======================================================================== */
#define execlp(a,b,...) ((void)0)

/* ========================================================================
 * Borland-specific ALLOC.H equivalents
 * ======================================================================== */

/* Nothing needed - malloc/free from stdlib.h suffice */

/* ========================================================================
 * Borland inline asm - replaced with macros that do nothing
 * The actual EGA register operations are replaced in ID_VW.C
 * ======================================================================== */
/* These were used as: asm { mov dx,GC_INDEX; ... }
 * In the SDL port, all VGA register access is removed.
 */

#endif /* ID_COMPAT_H */
