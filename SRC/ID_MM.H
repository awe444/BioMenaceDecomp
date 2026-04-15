/* Copyright (C) 2024 Nikolai Wuttke-Hohendorf
 *
 * Based on reconstructed Commander Keen 4-6 Source Code
 * Copyright (C) 2021 K1n9_Duk3
 *
 * This file is primarily based on:
 * Catacomb 3-D Source Code
 * Copyright (C) 1993-2014 Flat Rock Software
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

// ID_MM.H

#ifndef ID_MM_H
#define ID_MM_H

#define BUFFERSIZE    0x1000    // miscellaneous, always available buffer

//==========================================================================

// memptr is defined as void* in ID_COMPAT.H

typedef struct
{
  int32_t nearheap,farheap,EMSmem,XMSmem,mainmem;
} mminfotype;

//==========================================================================

extern  mminfotype  mminfo;
extern  memptr    bufferseg;
extern  boolean   mmerror;

extern  void    (* beforesort) (void);
extern  void    (* aftersort) (void);

//==========================================================================

void MM_Startup (void);
void MM_Shutdown (void);
void MM_MapEMS (void);

void MM_GetPtr (memptr *baseptr,unsigned long size);
void MM_FreePtr (memptr *baseptr);

void MM_SetPurge (memptr *baseptr, int purge);
void MM_SetLock (memptr *baseptr, boolean locked);
void MM_SortMem (void);

void MM_ShowMemory (void);

long MM_UnusedMemory (void);
long MM_TotalFree (void);

void MM_BombOnError (boolean bomb);

#endif
