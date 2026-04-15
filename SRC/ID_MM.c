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

// ID_MM.C - SDL port
//
// Replaces DOS memory manager (EMS/XMS/near heap/far heap) with
// simple malloc/free wrappers for modern flat-memory systems.

#include "ID_HEADS.H"

/*
=============================================================================

             GLOBAL VARIABLES

=============================================================================
*/

mminfotype  mminfo;
memptr    bufferseg;
boolean   mmerror;

void    (* beforesort) (void);
void    (* aftersort) (void);

/*
=============================================================================

             LOCAL VARIABLES

=============================================================================
*/

static boolean  mmstarted;
static boolean  bombonerror;

//==========================================================================

/*
===================
=
= MM_Startup
=
= Allocates the misc buffer and reports available memory
=
===================
*/

void MM_Startup (void)
{
  if (mmstarted)
    MM_Shutdown ();

  mmstarted = true;
  bombonerror = true;
  mmerror = false;

  beforesort = NULL;
  aftersort = NULL;

  mminfo.nearheap = 0;
  mminfo.farheap = 0;
  mminfo.EMSmem = 0;
  mminfo.XMSmem = 0;
  mminfo.mainmem = 1024L * 1024L; // report 1 MB available

  MM_GetPtr (&bufferseg, BUFFERSIZE);
}

//==========================================================================

/*
====================
=
= MM_Shutdown
=
= Frees the misc buffer
=
====================
*/

void MM_Shutdown (void)
{
  if (!mmstarted)
    return;

  if (bufferseg)
  {
    free (bufferseg);
    bufferseg = NULL;
  }

  mmstarted = false;
}

//==========================================================================

/*
====================
=
= MM_GetPtr
=
= Allocates a block of memory
=
====================
*/

void MM_GetPtr (memptr *baseptr, unsigned long size)
{
  *baseptr = malloc (size);

  if (!*baseptr)
  {
    if (bombonerror)
      Quit ("MM_GetPtr: Out of memory!");
    else
      mmerror = true;
  }
}

//==========================================================================

/*
====================
=
= MM_FreePtr
=
= Frees a previously allocated block
=
====================
*/

void MM_FreePtr (memptr *baseptr)
{
  if (*baseptr)
  {
    free (*baseptr);
    *baseptr = NULL;
  }
}

//==========================================================================

/*
=====================
=
= MM_SetPurge
=
= No-op in SDL port (modern systems don't need purge levels)
=
=====================
*/

void MM_SetPurge (memptr *baseptr, int purge)
{
  (void)baseptr;
  (void)purge;
}

//==========================================================================

/*
=====================
=
= MM_SetLock
=
= No-op in SDL port (modern systems don't need memory locking)
=
=====================
*/

void MM_SetLock (memptr *baseptr, boolean locked)
{
  (void)baseptr;
  (void)locked;
}

//==========================================================================

/*
=====================
=
= MM_SortMem
=
= No-op in SDL port (flat memory model, no compaction needed)
=
=====================
*/

void MM_SortMem (void)
{
}

//==========================================================================

/*
=====================
=
= MM_ShowMemory
=
= No-op in SDL port
=
=====================
*/

void MM_ShowMemory (void)
{
}

//==========================================================================

/*
======================
=
= MM_UnusedMemory
=
= Returns a large value since modern systems have plenty of memory
=
======================
*/

long MM_UnusedMemory (void)
{
  return 1024L * 1024L;
}

//==========================================================================

/*
======================
=
= MM_TotalFree
=
= Returns a large value since modern systems have plenty of memory
=
======================
*/

long MM_TotalFree (void)
{
  return 1024L * 1024L;
}

//==========================================================================

/*
=====================
=
= MM_BombOnError
=
=====================
*/

void MM_BombOnError (boolean bomb)
{
  bombonerror = bomb;
}

//==========================================================================

/*
====================
=
= MM_MapEMS
=
= No-op in SDL port (no EMS)
=
====================
*/

void MM_MapEMS (void)
{
}



//==========================================================================

/*
====================
=
= MML_UseSpace
=
= No-op in SDL port (no segment-based memory reclamation)
=
====================
*/

void MML_UseSpace (unsigned segstart, unsigned seglength)
{
  (void)segstart;
  (void)seglength;
}
