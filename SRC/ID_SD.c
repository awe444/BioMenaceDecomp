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

#include "opl.h"

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

// Thread-safe audio device locking macros
#define SD_LockAudio()   do { if (sdl_audioStarted) SDL_LockAudioDevice(sdl_audioDevice); } while(0)
#define SD_UnlockAudio() do { if (sdl_audioStarted) SDL_UnlockAudioDevice(sdl_audioDevice); } while(0)

//  Global variables
  boolean   SoundSourcePresent,SoundBlasterPresent,AdLibPresent,QuietFX,
        NeedsDigitized,NeedsMusic;
  SDMode    SoundMode;
  SMMode    MusicMode;
  longword  TimeCount;
  word    HackCount;
  word    *SoundTable;  // Really * _seg *SoundTable, but that don't work
  word    SoundTableOffset;  // SDL port: base index into audiosegs[], so audiosegs[SoundTableOffset + sound] gives the sound data
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

// SDL2 audio output state
#define SD_SAMPLE_RATE    49716   // Native OPL sample rate
#define SD_SFX_RATE       140     // Sound effect / PC speaker service rate (Hz)
#define SD_MUSIC_RATE     700     // AdLib music sequencer rate (Hz); = TickBase*10 per Wolf3D ID_SD
#define SD_SFX_TICKS_PER_MUSIC_TICK (SD_MUSIC_RATE / SD_SFX_RATE) // SFX fires every Nth music tick
#define PC_PIT_RATE       1193182 // PC PIT base clock

static  SDL_AudioDeviceID sdl_audioDevice;
static  boolean     sdl_audioStarted;
static  int       sdl_samplesPerMusicTick; // Samples between 700Hz music ticks (AdLib music on)
static  int       sdl_samplesPerSfxTick;   // Samples between 140Hz ticks (music off or SFX only)
static  int       sdl_sampleCounter;       // Counts down to next service tick
static  int       sdl_sfxTickCounter;      // When music on: run SFX every SD_SFX_TICKS_PER_MUSIC_TICK music ticks

// PC Speaker emulation state
static  boolean     sdl_pcSpkActive;
static  int16_t     sdl_pcSpkSample;
static  uint32_t    sdl_pcSpkCounter;
static  uint32_t    sdl_pcSpkPeriod;     // Half-cycle period in samples * PIT_RATE

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
//    emulate the original 70Hz timer interrupt.
//    Must be called regularly from the main thread (e.g. via IN_PumpEvents)
//    because game loops spin on TimeCount for frame timing.
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

// Forward declaration - defined after AdLib code with SDL2 audio callback
static void SDL_SetPCSpk(word value);

///////////////////////////////////////////////////////////////////////////
//
//  SDL_PCPlaySound() - Plays the specified sound on the PC speaker
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
//    Updates the PC speaker square wave emulation
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
    {
      pcLastSample = s;
      SDL_SetPCSpk(pcSoundLookup[s]);
    }

    if (!(--pcLengthLeft))
    {
      SDL_PCStopSound();
      SDL_SoundFinished();
      SDL_SetPCSpk(0);
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
  SDL_SetPCSpk(0);
}

//  AdLib Code

///////////////////////////////////////////////////////////////////////////
//
//  alOut(n,b) - Puts b in AdLib card register n
//    Writes to the OPL emulator
//
///////////////////////////////////////////////////////////////////////////
void
alOut(byte n,byte b)
{
  OPL_WriteReg(n, b);
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
//  SDL2 Audio callback and PC speaker emulation
//
///////////////////////////////////////////////////////////////////////////

//
//  SDL_PCSpkMix() - Mixes PC speaker square wave into an existing buffer
//
static void
SDL_PCSpkMix(int16_t *buffer, int length)
{
  int i;

  if (!sdl_pcSpkActive || sdl_pcSpkPeriod == 0)
    return;

  for (i = 0; i < length; i++)
  {
    int32_t mixed = (int32_t)buffer[i] + (int32_t)sdl_pcSpkSample;
    if (mixed > 32767)  mixed = 32767;
    if (mixed < -32768) mixed = -32768;
    buffer[i] = (int16_t)mixed;

    sdl_pcSpkCounter += 2 * PC_PIT_RATE;
    if (sdl_pcSpkCounter >= sdl_pcSpkPeriod)
    {
      sdl_pcSpkCounter %= sdl_pcSpkPeriod;
      sdl_pcSpkSample = 8192 - sdl_pcSpkSample;  // Toggle between 0 and 8192
    }
  }
}

//
//  SD_SDL_AudioCB() - SDL2 audio callback, generates OPL + PC speaker audio
//    and runs hardware-timed service routines:
//    With AdLib music (Wolf3D-style): music sequencer at 700Hz, SFX at 140Hz (every 5th tick).
//    Otherwise: SFX at 140Hz only (matches slow timer path in original ID_SD).
//
static void
SD_SDL_AudioCB(void *userdata, Uint8 *stream, int len)
{
  int16_t *out = (int16_t *)stream;
  int   totalSamples = len / sizeof(int16_t);
  int   samplesPerTick;

  (void)userdata;

  while (totalSamples > 0)
  {
    int chunk;

    samplesPerTick = (MusicMode == smm_AdLib) ? sdl_samplesPerMusicTick : sdl_samplesPerSfxTick;

    // If we've counted down to zero, fire the service routines
    if (sdl_sampleCounter <= 0)
    {
      if (MusicMode == smm_AdLib)
      {
        // Music sequencer (alTimeCount); must match original ~700Hz rate
        SDL_ALService();

        // Sound effects at 140Hz: every SD_SFX_TICKS_PER_MUSIC_TICK music ticks (see Wolf3D SDL_t0Service)
        if (--sdl_sfxTickCounter <= 0)
        {
          sdl_sfxTickCounter = SD_SFX_TICKS_PER_MUSIC_TICK;
          switch (SoundMode)
          {
          case sdm_PC:
            SDL_PCService();
            break;
          case sdm_AdLib:
            SDL_ALSoundService();
            break;
          }
        }
      }
      else
      {
        switch (SoundMode)
        {
        case sdm_PC:
          SDL_PCService();
          break;
        case sdm_AdLib:
          SDL_ALSoundService();
          break;
        }
      }

      sdl_sampleCounter += samplesPerTick;
    }

    // Generate samples up to the next service tick (or end of buffer)
    chunk = sdl_sampleCounter;
    if (chunk > totalSamples)
      chunk = totalSamples;

    // Generate OPL samples
    OPL_GenerateSamples(out, (uint32_t)chunk);

    // Mix in PC speaker square wave
    SDL_PCSpkMix(out, chunk);

    out += chunk;
    totalSamples -= chunk;
    sdl_sampleCounter -= chunk;
  }
}

//
//  SDL_SetPCSpk() - Updates PC speaker emulation state based on pcSoundLookup
//
static void
SDL_SetPCSpk(word value)
{
  if (value)
  {
    sdl_pcSpkActive = true;
    sdl_pcSpkPeriod = (uint32_t)SD_SAMPLE_RATE * value;
    sdl_pcSpkCounter = 0;
    sdl_pcSpkSample = 8192;
  }
  else
  {
    sdl_pcSpkActive = false;
  }
}

//
//  SDL_StartAudio() - Opens the SDL2 audio device
//
static void
SDL_StartAudio(void)
{
  SDL_AudioSpec desired, obtained;

  if (sdl_audioStarted)
    return;

  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
  {
    fprintf(stderr, "SD: SDL_InitSubSystem(AUDIO) failed: %s\n", SDL_GetError());
    return;
  }

  OPL_Init(SD_SAMPLE_RATE);

  memset(&desired, 0, sizeof(desired));
  desired.freq = SD_SAMPLE_RATE;
  desired.format = AUDIO_S16SYS;
  desired.channels = 1;
  desired.samples = 512;
  desired.callback = SD_SDL_AudioCB;

  sdl_audioDevice = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
  if (sdl_audioDevice == 0)
  {
    fprintf(stderr, "SD: SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
    return;
  }

  sdl_samplesPerMusicTick = obtained.freq / SD_MUSIC_RATE;
  sdl_samplesPerSfxTick = sdl_samplesPerMusicTick * SD_SFX_TICKS_PER_MUSIC_TICK;
  sdl_sfxTickCounter = SD_SFX_TICKS_PER_MUSIC_TICK;
  sdl_sampleCounter = sdl_samplesPerSfxTick;

  // Unpause the audio device to start playback
  SDL_PauseAudioDevice(sdl_audioDevice, 0);
  sdl_audioStarted = true;
}

//
//  SDL_StopAudio() - Closes the SDL2 audio device
//
static void
SDL_StopAudio(void)
{
  if (!sdl_audioStarted)
    return;

  SDL_CloseAudioDevice(sdl_audioDevice);
  sdl_audioDevice = 0;
  sdl_audioStarted = false;

  SDL_QuitSubSystem(SDL_INIT_AUDIO);

  OPL_Shutdown();
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
    SoundTableOffset = tableoffset;
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
  {
    if (sdl_audioStarted)
    {
      SD_LockAudio();
      MusicMode = mode;
      if (MusicMode == smm_AdLib)
      {
        sdl_sampleCounter = sdl_samplesPerMusicTick;
        sdl_sfxTickCounter = SD_SFX_TICKS_PER_MUSIC_TICK;
      }
      else
      {
        sdl_sampleCounter = sdl_samplesPerSfxTick;
      }
      SD_UnlockAudio();
    }
    else
    {
      MusicMode = mode;
    }
  }

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
  HackCount = 0;

  // Start SDL2 audio output
  SDL_StartAudio();

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

  SDL_StopAudio();

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

  s = (SoundCommon far *)audiosegs[SoundTableOffset + sound];
  if (!s)
    Quit("SD_PlaySound() - Uncached sound");
  if (!s->length)
    Quit("SD_PlaySound() - Zero length sound");
  if (s->priority < SoundPriority)
    return;

  SD_LockAudio();

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

  SD_UnlockAudio();
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
  SD_LockAudio();

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

  SD_UnlockAudio();
}

///////////////////////////////////////////////////////////////////////////
//
//  SD_WaitSoundDone() - waits until the current sound is done playing
//
///////////////////////////////////////////////////////////////////////////
void
SD_WaitSoundDone(void)
{
  while (SD_SoundPlaying())
    SDL_Delay(10);
}

///////////////////////////////////////////////////////////////////////////
//
//  SD_MusicOn() - turns on the sequencer
//
///////////////////////////////////////////////////////////////////////////
void
SD_MusicOn(void)
{
  SD_LockAudio();
  sqActive = true;
  SD_UnlockAudio();
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

  SD_LockAudio();

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

  SD_UnlockAudio();
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

  SD_LockAudio();

  if (MusicMode == smm_AdLib)
  {
    sqHackPtr = sqHack = music->values;
    sqHackSeqLen = sqHackLen = music->length;
    sqHackTime = 0;
    alTimeCount = 0;
    sqActive = true;
  }

  SD_UnlockAudio();
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
