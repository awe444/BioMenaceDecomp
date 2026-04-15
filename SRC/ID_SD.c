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

//
//  ID Engine
//  ID_SD.c - Sound Manager - SDL2 stub version
//  v1.1d1
//  By Jason Blochowiak
//

//
//  This module handles dealing with generating sound on the appropriate
//    hardware
//
//  Depends on: User Mgr (for parm checking)
//
//  Globals:
//    For User Mgr:
//      SoundSourcePresent - Sound Source thingie present?
//      SoundBlasterPresent - SoundBlaster card present?
//      AdLibPresent - AdLib card present?
//      SoundMode - What device is used for sound effects
//        (Use SM_SetSoundMode() to set)
//      MusicMode - What device is used for music
//        (Use SM_SetMusicMode() to set)
//    For Cache Mgr:
//      NeedsDigitized - load digitized sounds?
//      NeedsMusic - load music?
//

#include <SDL.h>

#ifdef  _MUSE_      // Will be defined in ID_Types.h
#include "ID_SD.h"
#else
#include "ID_HEADS.H"
#endif

// Borland C provided _argc/_argv as automatic globals.
// In the SDL port these are provided by the main translation unit.
extern int _argc;
extern char **_argv;

#define SDL_SoundFinished() {SoundNumber = SoundPriority = 0;}

//  Global variables
  boolean   SoundSourcePresent,SoundBlasterPresent,AdLibPresent,QuietFX,
        NeedsDigitized,NeedsMusic;
  SDMode    SoundMode;
  SMMode    MusicMode;
  longword  TimeCount;
  word    HackCount;
  word    *SoundTable;  // Really * _seg *SoundTable, but that don't work
  boolean   ssIsTandy;
  word    ssPort = 2;

//  Internal variables
static  boolean     SD_Started;
static  char      *ParmStrings[] =
            {
              "noal",
              nil
            };
static  void      (*SoundUserHook)(void);
static  word      SoundNumber,SoundPriority;
static  long      LocalTime;

// SDL2 timing state for 70Hz TimeCount emulation
static  Uint32      sdl_lastTicks;
static  Uint32      sdl_tickRemainder;

//  PC Sound variables
static  byte      pcLastSample,far *pcSound;
static  longword    pcLengthLeft;
static  word      pcSoundLookup[255];

//  AdLib variables
static  boolean     alNoCheck;
static  byte      far *alSound;
static  word      alBlock;
static  longword    alLengthLeft;
static  longword    alTimeCount;
static  Instrument    alZeroInst;

// This table maps channel numbers to carrier and modulator op cells
static  byte      carriers[9] =  { 3, 4, 5,11,12,13,19,20,21},
            modifiers[9] = { 0, 1, 2, 8, 9,10,16,17,18},
// This table maps percussive voice numbers to op cells
            pcarriers[5] = {19,0xff,0xff,0xff,0xff},
            pmodifiers[5] = {16,17,18,20,21};

//  Sequencer variables
static  boolean     sqActive;
static  word      alFXReg;
static  ActiveTrack   *tracks[sqMaxTracks],
            mytracks[sqMaxTracks];
static  word      sqMode,sqFadeStep;
static  word      far *sqHack,far *sqHackPtr,sqHackLen,sqHackSeqLen;
static  long      sqHackTime;

//  Internal routines

///////////////////////////////////////////////////////////////////////////
//
//  SD_UpdateTimeCount() - Updates TimeCount based on SDL_GetTicks() to
//    emulate the original 70Hz timer interrupt
//
///////////////////////////////////////////////////////////////////////////
void
SD_UpdateTimeCount(void)
{
  Uint32  now = SDL_GetTicks();
  Uint32  elapsed_ms = now - sdl_lastTicks;
  Uint32  total, ticks;

  sdl_lastTicks = now;

  // Convert elapsed milliseconds to 70Hz ticks with remainder tracking
  total = elapsed_ms * 70 + sdl_tickRemainder;
  ticks = total / 1000;
  sdl_tickRemainder = total % 1000;

  TimeCount += ticks;
  LocalTime += ticks;

  if (ticks > 0 && SoundUserHook)
    SoundUserHook();
}

///////////////////////////////////////////////////////////////////////////
//
//  SDL_SetTimerSpeed() - No-op in SDL2 stub (no hardware timer to program)
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_SetTimerSpeed(void)
{
}

//
//  PC Sound code
//

///////////////////////////////////////////////////////////////////////////
//
//  SDL_PCPlaySound() - Plays the specified sound on the PC speaker
//    (stub: records state only, no actual audio)
//
///////////////////////////////////////////////////////////////////////////
#ifdef  _MUSE_
void
#else
static void
#endif
SDL_PCPlaySound(PCSound far *sound)
{
  pcLastSample = -1;
  pcLengthLeft = sound->common.length;
  pcSound = sound->data;
}

///////////////////////////////////////////////////////////////////////////
//
//  SDL_PCStopSound() - Stops the current sound playing on the PC Speaker
//
///////////////////////////////////////////////////////////////////////////
#ifdef  _MUSE_
void
#else
static void
#endif
SDL_PCStopSound(void)
{
  pcSound = 0;
}

///////////////////////////////////////////////////////////////////////////
//
//  SDL_PCService() - Handles playing the next sample in a PC sound
//    (stub: advances state only, no actual audio output)
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_PCService(void)
{
  byte  s;

  if (pcSound)
  {
    s = *pcSound++;
    if (s != pcLastSample)
      pcLastSample = s;

    if (!(--pcLengthLeft))
    {
      SDL_PCStopSound();
      SDL_SoundFinished();
    }
  }
}

///////////////////////////////////////////////////////////////////////////
//
//  SDL_ShutPC() - Turns off the pc speaker
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_ShutPC(void)
{
  pcSound = 0;
}

//  AdLib Code

///////////////////////////////////////////////////////////////////////////
//
//  alOut(n,b) - Puts b in AdLib card register n
//    (stub: no-op, no hardware to write to)
//
///////////////////////////////////////////////////////////////////////////
void
alOut(byte n,byte b)
{
  (void)n;
  (void)b;
}

#if 0
///////////////////////////////////////////////////////////////////////////
//
//  SDL_SetInstrument() - Puts an instrument into a generator
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_SetInstrument(int track,int which,Instrument far *inst,boolean percussive)
{
  byte    c,m;

  if (percussive)
  {
    c = pcarriers[which];
    m = pmodifiers[which];
  }
  else
  {
    c = carriers[which];
    m = modifiers[which];
  }

  tracks[track - 1]->inst = *inst;
  tracks[track - 1]->percussive = percussive;

  alOut(m + alChar,inst->mChar);
  alOut(m + alScale,inst->mScale);
  alOut(m + alAttack,inst->mAttack);
  alOut(m + alSus,inst->mSus);
  alOut(m + alWave,inst->mWave);

  // Most percussive instruments only use one cell
  if (c != 0xff)
  {
    alOut(c + alChar,inst->cChar);
    alOut(c + alScale,inst->cScale);
    alOut(c + alAttack,inst->cAttack);
    alOut(c + alSus,inst->cSus);
    alOut(c + alWave,inst->cWave);
  }

  alOut(which + alFeedCon,inst->nConn); // DEBUG - I think this is right
}
#endif

///////////////////////////////////////////////////////////////////////////
//
//  SDL_ALStopSound() - Turns off any sound effects playing through the
//    AdLib card
//
///////////////////////////////////////////////////////////////////////////
#ifdef  _MUSE_
void
#else
static void
#endif
SDL_ALStopSound(void)
{
  alSound = 0;
  alOut(alFreqH + 0,0);
}

static void
SDL_AlSetFXInst(Instrument far *inst)
{
  byte    c,m;
  byte    scale;  // added for "quiet AdLib" mode

  m = modifiers[0];
  c = carriers[0];
  alOut(m + alChar,inst->mChar);
  alOut(m + alScale,inst->mScale);
  alOut(m + alAttack,inst->mAttack);
  alOut(m + alSus,inst->mSus);
  alOut(m + alWave,inst->mWave);
  alOut(c + alChar,inst->cChar);
#if 1
  // quiet AdLib code:
  scale = inst->cScale;
  if (QuietFX)
  {
    scale = 0x3F-scale;
    scale = (scale>>1) + (scale>>2);  // basically 'scale *= 0.75;'
    scale = 0x3F-scale;
  }
  alOut(c + alScale,scale);
#else
  // old code:
  alOut(c + alScale,inst->cScale);
#endif
  alOut(c + alAttack,inst->cAttack);
  alOut(c + alSus,inst->cSus);
  alOut(c + alWave,inst->cWave);
  // DEBUG!!! - I just put this in
//  alOut(alFeedCon,inst->nConn);
}

///////////////////////////////////////////////////////////////////////////
//
//  SDL_ALPlaySound() - Plays the specified sound on the AdLib card
//
///////////////////////////////////////////////////////////////////////////
#ifdef  _MUSE_
void
#else
static void
#endif
SDL_ALPlaySound(AdLibSound far *sound)
{
  Instrument  far *inst;

  SDL_ALStopSound();

  alLengthLeft = sound->common.length;
  alSound = sound->data;
  alBlock = ((sound->block & 7) << 2) | 0x20;
  inst = &sound->inst;

  if (!(inst->mSus | inst->cSus))
  {
    Quit("SDL_ALPlaySound() - Bad instrument");
  }

  SDL_AlSetFXInst(inst);
}

///////////////////////////////////////////////////////////////////////////
//
//  SDL_ALSoundService() - Plays the next sample out through the AdLib card
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_ALSoundService(void)
{
  byte  s;

  if (alSound)
  {
    s = *alSound++;
    if (!s)
      alOut(alFreqH + 0,0);
    else
    {
      alOut(alFreqL + 0,s);
      alOut(alFreqH + 0,alBlock);
    }

    if (!(--alLengthLeft))
    {
      alSound = 0;
      alOut(alFreqH + 0,0);
      SDL_SoundFinished();
    }
  }
}

#if 0
///////////////////////////////////////////////////////////////////////////
//
//  SDL_SelectMeasure() - sets up sequencing variables for a given track
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_SelectMeasure(ActiveTrack *track)
{
  track->seq = track->moods[track->mood];
  track->nextevent = 0;
}
#endif

static void
SDL_ALService(void)
{
  byte  a,v;
  word  w;

  if (!sqActive)
    return;

  while (sqHackLen && (sqHackTime <= alTimeCount))
  {
    w = *sqHackPtr++;
    sqHackTime = alTimeCount + *sqHackPtr++;
    a = w & 0xFF;
    v = (w >> 8) & 0xFF;
    alOut(a,v);
    sqHackLen -= 4;
  }
  alTimeCount++;
  if (!sqHackLen)
  {
    sqHackPtr = (word far *)sqHack;
    sqHackLen = sqHackSeqLen;
    alTimeCount = sqHackTime = 0;
  }
}

///////////////////////////////////////////////////////////////////////////
//
//  SDL_ShutAL() - Shuts down the AdLib card for sound effects
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_ShutAL(void)
{
  alOut(alEffects,0);
  alOut(alFreqH + 0,0);
  SDL_AlSetFXInst(&alZeroInst);
  alSound = 0;
}

///////////////////////////////////////////////////////////////////////////
//
//  SDL_CleanAL() - Totally shuts down the AdLib card
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_CleanAL(void)
{
  int i;

  alOut(alEffects,0);
  for (i = 1;i < 0xf5;i++)
    alOut(i,0);
}

///////////////////////////////////////////////////////////////////////////
//
//  SDL_StartAL() - Starts up the AdLib card for sound effects
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_StartAL(void)
{
  alFXReg = 0;
  alOut(alEffects,alFXReg);
  SDL_AlSetFXInst(&alZeroInst);
}

///////////////////////////////////////////////////////////////////////////
//
//  SDL_DetectAdLib() - In the SDL2 stub, always reports AdLib as present
//
///////////////////////////////////////////////////////////////////////////
static boolean
SDL_DetectAdLib(void)
{
  return(true);
}

////////////////////////////////////////////////////////////////////////////
//
//  SDL_ShutDevice() - turns off whatever device was being used for sound fx
//
////////////////////////////////////////////////////////////////////////////
static void
SDL_ShutDevice(void)
{
  switch (SoundMode)
  {
  case sdm_PC:
    SDL_ShutPC();
    break;
  case sdm_AdLib:
    SDL_ShutAL();
    break;
  }
  SoundMode = sdm_Off;
}

///////////////////////////////////////////////////////////////////////////
//
//  SDL_CleanDevice() - totally shuts down all sound devices
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_CleanDevice(void)
{
  if ((SoundMode == sdm_AdLib) || (MusicMode == smm_AdLib))
    SDL_CleanAL();
}

///////////////////////////////////////////////////////////////////////////
//
//  SDL_StartDevice() - turns on whatever device is to be used for sound fx
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_StartDevice(void)
{
  switch (SoundMode)
  {
  case sdm_AdLib:
    SDL_StartAL();
    break;
  }
  SoundNumber = SoundPriority = 0;
}

//  Public routines

///////////////////////////////////////////////////////////////////////////
//
//  SD_SetSoundMode() - Sets which sound hardware to use for sound effects
//
///////////////////////////////////////////////////////////////////////////
boolean
SD_SetSoundMode(SDMode mode)
{
  boolean result = false;
  word  tableoffset;

  SD_StopSound();

#ifndef _MUSE_
  switch (mode)
  {
  case sdm_Off:
    NeedsDigitized = false;
    result = true;
    break;
  case sdm_PC:
    tableoffset = STARTPCSOUNDS;
    NeedsDigitized = false;
    result = true;
    break;
  case sdm_AdLib:
    if (AdLibPresent)
    {
      tableoffset = STARTADLIBSOUNDS;
      NeedsDigitized = false;
      result = true;
    }
    break;
  default:
    result = false;
    break;
  }
#endif

  if (result && (mode != SoundMode))
  {
    SDL_ShutDevice();
    SoundMode = mode;
#ifndef _MUSE_
    SoundTable = (word *)(&audiosegs[tableoffset]);
#endif
    SDL_StartDevice();
  }

  SDL_SetTimerSpeed();

  return(result);
}

///////////////////////////////////////////////////////////////////////////
//
//  SD_SetMusicMode() - sets the device to use for background music
//
///////////////////////////////////////////////////////////////////////////
boolean
SD_SetMusicMode(SMMode mode)
{
  boolean result = false;

  SD_FadeOutMusic();
  while (SD_MusicPlaying())
    ;

  switch (mode)
  {
  case smm_Off:
    NeedsMusic = false;
    result = true;
    break;
  case smm_AdLib:
    if (AdLibPresent)
    {
      NeedsMusic = true;
      result = true;
    }
    break;
  default:
    result = false;
    break;
  }

  if (result)
    MusicMode = mode;

  SDL_SetTimerSpeed();

  return(result);
}

///////////////////////////////////////////////////////////////////////////
//
//  SD_Startup() - starts up the Sound Mgr
//    Detects all additional sound hardware
//
///////////////////////////////////////////////////////////////////////////
void
SD_Startup(void)
{
  int i;

  if (SD_Started)
    return;

  ssIsTandy = false;
  alNoCheck = false;
#ifndef _MUSE_
  for (i = 1;i < _argc;i++)
  {
    switch (US_CheckParm(_argv[i],ParmStrings))
    {
    case 0:           // No AdLib detection
      alNoCheck = true;
      break;
    }
  }
#endif

  SoundUserHook = 0;

  sdl_lastTicks = SDL_GetTicks();
  sdl_tickRemainder = 0;
  LocalTime = TimeCount = alTimeCount = 0;

  SD_SetSoundMode(sdm_Off);
  SD_SetMusicMode(smm_Off);

  if (!alNoCheck)
    AdLibPresent = SDL_DetectAdLib();

  for (i = 0;i < 255;i++)
    pcSoundLookup[i] = i * 60;

  SD_Started = true;
}

///////////////////////////////////////////////////////////////////////////
//
//  SD_Default() - Sets up the default behaviour for the Sound Mgr whether
//    the config file was present or not.
//
///////////////////////////////////////////////////////////////////////////
void
SD_Default(boolean gotit,SDMode sd,SMMode sm)
{
  boolean gotsd,gotsm;

  gotsd = gotsm = gotit;

  if (gotsd)  // Make sure requested sound hardware is available
  {
    switch (sd)
    {
    case sdm_AdLib:
      gotsd = AdLibPresent;
      break;
    }
  }
  if (!gotsd)
  {
    if (AdLibPresent)
      sd = sdm_AdLib;
    else
      sd = sdm_PC;
  }
  if (sd != SoundMode)
    SD_SetSoundMode(sd);


  if (gotsm)  // Make sure requested music hardware is available
  {
    switch (sm)
    {
    case sdm_AdLib:   // BUG: this should use smm_AdLib!
      gotsm = AdLibPresent;
      break;
    }
  }
  if (!gotsm)
  {
    if (AdLibPresent)
      sm = smm_AdLib;
    else
      sm = smm_Off;
  }
  if (sm != MusicMode)
    SD_SetMusicMode(sm);
}

///////////////////////////////////////////////////////////////////////////
//
//  SD_Shutdown() - shuts down the Sound Mgr
//    Turns off whatever sound hardware was active
//
///////////////////////////////////////////////////////////////////////////
void
SD_Shutdown(void)
{
  if (!SD_Started)
    return;

  SD_MusicOff();
  SDL_ShutDevice();
  SDL_CleanDevice();

  SD_Started = false;
}

///////////////////////////////////////////////////////////////////////////
//
//  SD_SetUserHook() - sets the routine that the Sound Mgr calls every 1/70th
//    of a second from its timer 0 ISR
//
///////////////////////////////////////////////////////////////////////////
void
SD_SetUserHook(void (* hook)(void))
{
  SoundUserHook = hook;
}

///////////////////////////////////////////////////////////////////////////
//
//  SD_PlaySound() - plays the specified sound on the appropriate hardware
//
///////////////////////////////////////////////////////////////////////////
void
SD_PlaySound(soundnames sound)
{
  SoundCommon far *s;

  if ((SoundMode == sdm_Off) /*|| (sound == -1)*/)
    return;

  s = MK_FP(SoundTable[sound],0);
  if (!s)
    Quit("SD_PlaySound() - Uncached sound");
  if (!s->length)
    Quit("SD_PlaySound() - Zero length sound");
  if (s->priority < SoundPriority)
    return;

  switch (SoundMode)
  {
  case sdm_PC:
    SDL_PCPlaySound((void far *)s);
    break;
  case sdm_AdLib:
    SDL_ALPlaySound((void far *)s);
    break;
  }

  SoundNumber = sound;
  SoundPriority = s->priority;
}

///////////////////////////////////////////////////////////////////////////
//
//  SD_SoundPlaying() - returns the sound number that's playing, or 0 if
//    no sound is playing
//
///////////////////////////////////////////////////////////////////////////
word
SD_SoundPlaying(void)
{
  boolean result = false;

  switch (SoundMode)
  {
  case sdm_PC:
    result = pcSound? true : false;
    break;
  case sdm_AdLib:
    result = alSound? true : false;
    break;
  }

  if (result)
    return(SoundNumber);
  else
    return(false);
}

///////////////////////////////////////////////////////////////////////////
//
//  SD_StopSound() - if a sound is playing, stops it
//
///////////////////////////////////////////////////////////////////////////
void
SD_StopSound(void)
{
  switch (SoundMode)
  {
  case sdm_PC:
    SDL_PCStopSound();
    break;
  case sdm_AdLib:
    SDL_ALStopSound();
    break;
  }

  SDL_SoundFinished();
}

///////////////////////////////////////////////////////////////////////////
//
//  SD_WaitSoundDone() - waits until the current sound is done playing
//    (stub: stops sound immediately since there is no audio playback)
//
///////////////////////////////////////////////////////////////////////////
void
SD_WaitSoundDone(void)
{
  SD_StopSound();
}

///////////////////////////////////////////////////////////////////////////
//
//  SD_MusicOn() - turns on the sequencer
//
///////////////////////////////////////////////////////////////////////////
void
SD_MusicOn(void)
{
  sqActive = true;
}

///////////////////////////////////////////////////////////////////////////
//
//  SD_MusicOff() - turns off the sequencer and any playing notes
//
///////////////////////////////////////////////////////////////////////////
void
SD_MusicOff(void)
{
  word  i;


  switch (MusicMode)
  {
  case smm_AdLib:
    alFXReg = 0;
    alOut(alEffects,0);
    for (i = 0;i < sqMaxTracks;i++)
      alOut(alFreqH + i + 1,0);
    break;
  }
  sqActive = false;
}

///////////////////////////////////////////////////////////////////////////
//
//  SD_StartMusic() - starts playing the music pointed to
//
///////////////////////////////////////////////////////////////////////////
void
SD_StartMusic(MusicGroup far *music)
{
  SD_MusicOff();

  if (MusicMode == smm_AdLib)
  {
    sqHackPtr = sqHack = music->values;
    sqHackSeqLen = sqHackLen = music->length;
    sqHackTime = 0;
    alTimeCount = 0;
    SD_MusicOn();
  }
}

///////////////////////////////////////////////////////////////////////////
//
//  SD_FadeOutMusic() - starts fading out the music. Call SD_MusicPlaying()
//    to see if the fadeout is complete
//
///////////////////////////////////////////////////////////////////////////
void
SD_FadeOutMusic(void)
{
  switch (MusicMode)
  {
  case smm_AdLib:
    // DEBUG - quick hack to turn the music off
    SD_MusicOff();
    break;
  }
}

///////////////////////////////////////////////////////////////////////////
//
//  SD_MusicPlaying() - returns true if music is currently playing, false if
//    not
//
///////////////////////////////////////////////////////////////////////////
boolean
SD_MusicPlaying(void)
{
  boolean result;

  switch (MusicMode)
  {
  case smm_AdLib:
    result = false;
    // DEBUG - not written
    break;
  default:
    result = false;
  }

  return(result);
}
