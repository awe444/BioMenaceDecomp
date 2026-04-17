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
//  ID_IN.c - Input Manager
//  v1.0d1
//  By Jason Blochowiak
//  SDL2 port
//

//
//  This module handles dealing with the various input devices
//
//  Depends on: Memory Mgr (for demo recording), Sound Mgr (for timing stuff),
//        User Mgr (for command line parms)
//
//  Globals:
//    LastScan - The keyboard scan code of the last key pressed
//    LastASCII - The ASCII value of the last key pressed
//  DEBUG - there are more globals
//

#include "ID_HEADS.H"
#include <SDL.h>

// Borland C provided _argc/_argv as automatic globals.
// In the SDL port, these must be defined elsewhere (e.g. in main()).
extern int _argc;
extern char **_argv;

// Stuff for the joystick
#define JoyScaleMax   32768
#define JoyScaleShift 8
#define MaxJoyValue   5000

//  Global variables
    boolean   Keyboard[NumCodes],
          JoysPresent[MaxJoys],
          MousePresent;
    boolean   Paused;
    char    LastASCII;
    ScanCode  LastScan;
    KeyboardDef KbdDefs[MaxKbds] = {{0x1d,0x38,0x00,0x48,0x00,0x4b,0x4d,0x00,0x50,0x00}};
    JoystickDef JoyDefs[MaxJoys];
    ControlType Controls[MaxPlayers];

    boolean Latch;
    long  MouseDownCount;
    boolean LatchedButton0[MaxPlayers];
    boolean LatchedButton1[MaxPlayers];

    Demo    DemoMode = demo_Off;
    byte _seg *DemoBuffer;
    word    DemoOffset,DemoSize;

//  Internal variables
static  boolean   IN_Started;
static  boolean   CapsLock;
static  ScanCode  CurCode,LastCode;

static  SDL_Joystick *sdl_joysticks[MaxJoys];

static  byte        far ASCIINames[] =    // Unshifted ASCII for scan codes
          {
//   0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
  0  ,27 ,'1','2','3','4','5','6','7','8','9','0','-','=',8  ,9  ,  // 0
  'q','w','e','r','t','y','u','i','o','p','[',']',13 ,0  ,'a','s',  // 1
  'd','f','g','h','j','k','l',';',39 ,'`',0  ,92 ,'z','x','c','v',  // 2
  'b','n','m',',','.','/',0  ,'*',0  ,' ',0  ,0  ,0  ,0  ,0  ,0  ,  // 3
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,'7','8','9','-','4','5','6','+','1',  // 4
  '2','3','0',127,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,  // 5
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,  // 6
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0   // 7
          },
          far ShiftNames[] =    // Shifted ASCII for scan codes
          {
//   0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
  0  ,27 ,'!','@','#','$','%','^','&','*','(',')','_','+',8  ,9  ,  // 0
  'Q','W','E','R','T','Y','U','I','O','P','{','}',13 ,0  ,'A','S',  // 1
  'D','F','G','H','J','K','L',':',34 ,'~',0  ,'|','Z','X','C','V',  // 2
  'B','N','M','<','>','?',0  ,'*',0  ,' ',0  ,0  ,0  ,0  ,0  ,0  ,  // 3
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,'7','8','9','-','4','5','6','+','1',  // 4
  '2','3','0',127,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,  // 5
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,  // 6
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0     // 7
          },

          *ScanNames[] =    // Scan code names with single chars
          {
  "?","?","1","2","3","4","5","6","7","8","9","0","-","+","?","?",
  "Q","W","E","R","T","Y","U","I","O","P","[","]","|","?","A","S",
  "D","F","G","H","J","K","L",";","\"","?","?","?","Z","X","C","V",
  "B","N","M",",",".","/","?","?","?","?","?","?","?","?","?","?",
  "?","?","?","?","?","?","?","?","\xf","?","-","\x15","5","\x11","+","?",
  "\x13","?","?","?","?","?","?","?","?","?","?","?","?","?","?","?",
  "?","?","?","?","?","?","?","?","?","?","?","?","?","?","?","?",
  "?","?","?","?","?","?","?","?","?","?","?","?","?","?","?","?"
          },  // DEBUG - consolidate these
          far ExtScanCodes[] =  // Scan codes with >1 char names
          {
  1,0xe,0xf,0x1d,0x2a,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,
  0x3f,0x40,0x41,0x42,0x43,0x44,0x57,0x59,0x46,0x1c,0x36,
  0x37,0x38,0x47,0x49,0x4f,0x51,0x52,0x53,0x45,0x48,
  0x50,0x4b,0x4d,0x00
          },
          *ExtScanNames[] = // Names corresponding to ExtScanCodes
          {
  "Esc","BkSp","Tab","Ctrl","LShft","Space","CapsLk","F1","F2","F3","F4",
  "F5","F6","F7","F8","F9","F10","F11","F12","ScrlLk","Enter","RShft",
  "PrtSc","Alt","Home","PgUp","End","PgDn","Ins","Del","NumLk","Up",
  "Down","Left","Right",""
          };
static  Direction DirTable[] =    // Quick lookup for total direction
          {
            dir_NorthWest,  dir_North,  dir_NorthEast,
            dir_West,   dir_None, dir_East,
            dir_SouthWest,  dir_South,  dir_SouthEast
          };

static  void      (*INL_KeyHook)(void);

static  char      *ParmStrings[] = {"nojoys","nomouse",nil};

///////////////////////////////////////////////////////////////////////////
//
//  INL_SDLScanCodeToDOS() - Translates an SDL scancode to a DOS scancode
//
///////////////////////////////////////////////////////////////////////////
static byte
INL_SDLScanCodeToDOS(SDL_Scancode sc)
{
  switch (sc)
  {
  case SDL_SCANCODE_ESCAPE:     return 0x01;
  case SDL_SCANCODE_1:          return 0x02;
  case SDL_SCANCODE_2:          return 0x03;
  case SDL_SCANCODE_3:          return 0x04;
  case SDL_SCANCODE_4:          return 0x05;
  case SDL_SCANCODE_5:          return 0x06;
  case SDL_SCANCODE_6:          return 0x07;
  case SDL_SCANCODE_7:          return 0x08;
  case SDL_SCANCODE_8:          return 0x09;
  case SDL_SCANCODE_9:          return 0x0a;
  case SDL_SCANCODE_0:          return 0x0b;
  case SDL_SCANCODE_MINUS:      return 0x0c;
  case SDL_SCANCODE_EQUALS:     return 0x0d;
  case SDL_SCANCODE_BACKSPACE:  return 0x0e;
  case SDL_SCANCODE_TAB:        return 0x0f;
  case SDL_SCANCODE_Q:          return 0x10;
  case SDL_SCANCODE_W:          return 0x11;
  case SDL_SCANCODE_E:          return 0x12;
  case SDL_SCANCODE_R:          return 0x13;
  case SDL_SCANCODE_T:          return 0x14;
  case SDL_SCANCODE_Y:          return 0x15;
  case SDL_SCANCODE_U:          return 0x16;
  case SDL_SCANCODE_I:          return 0x17;
  case SDL_SCANCODE_O:          return 0x18;
  case SDL_SCANCODE_P:          return 0x19;
  case SDL_SCANCODE_LEFTBRACKET:  return 0x1a;
  case SDL_SCANCODE_RIGHTBRACKET: return 0x1b;
  case SDL_SCANCODE_RETURN:     return 0x1c;
  case SDL_SCANCODE_LCTRL:      return 0x1d;
  case SDL_SCANCODE_RCTRL:      return 0x1d;
  case SDL_SCANCODE_A:          return 0x1e;
  case SDL_SCANCODE_S:          return 0x1f;
  case SDL_SCANCODE_D:          return 0x20;
  case SDL_SCANCODE_F:          return 0x21;
  case SDL_SCANCODE_G:          return 0x22;
  case SDL_SCANCODE_H:          return 0x23;
  case SDL_SCANCODE_J:          return 0x24;
  case SDL_SCANCODE_K:          return 0x25;
  case SDL_SCANCODE_L:          return 0x26;
  case SDL_SCANCODE_SEMICOLON:  return 0x27;
  case SDL_SCANCODE_APOSTROPHE: return 0x28;
  case SDL_SCANCODE_GRAVE:      return 0x29;
  case SDL_SCANCODE_LSHIFT:     return 0x2a;
  case SDL_SCANCODE_BACKSLASH:  return 0x2b;
  case SDL_SCANCODE_Z:          return 0x2c;
  case SDL_SCANCODE_X:          return 0x2d;
  case SDL_SCANCODE_C:          return 0x2e;
  case SDL_SCANCODE_V:          return 0x2f;
  case SDL_SCANCODE_B:          return 0x30;
  case SDL_SCANCODE_N:          return 0x31;
  case SDL_SCANCODE_M:          return 0x32;
  case SDL_SCANCODE_COMMA:      return 0x33;
  case SDL_SCANCODE_PERIOD:     return 0x34;
  case SDL_SCANCODE_SLASH:      return 0x35;
  case SDL_SCANCODE_RSHIFT:     return 0x36;
  case SDL_SCANCODE_KP_MULTIPLY:return 0x37;
  case SDL_SCANCODE_LALT:       return 0x38;
  case SDL_SCANCODE_RALT:       return 0x38;
  case SDL_SCANCODE_SPACE:      return 0x39;
  case SDL_SCANCODE_CAPSLOCK:   return 0x3a;
  case SDL_SCANCODE_F1:         return 0x3b;
  case SDL_SCANCODE_F2:         return 0x3c;
  case SDL_SCANCODE_F3:         return 0x3d;
  case SDL_SCANCODE_F4:         return 0x3e;
  case SDL_SCANCODE_F5:         return 0x3f;
  case SDL_SCANCODE_F6:         return 0x40;
  case SDL_SCANCODE_F7:         return 0x41;
  case SDL_SCANCODE_F8:         return 0x42;
  case SDL_SCANCODE_F9:         return 0x43;
  case SDL_SCANCODE_F10:        return 0x44;
  case SDL_SCANCODE_NUMLOCKCLEAR: return 0x45;
  case SDL_SCANCODE_SCROLLLOCK: return 0x46;
  case SDL_SCANCODE_KP_7:       return 0x47;
  case SDL_SCANCODE_HOME:       return 0x47;
  case SDL_SCANCODE_KP_8:       return 0x48;
  case SDL_SCANCODE_UP:         return 0x48;
  case SDL_SCANCODE_KP_9:       return 0x49;
  case SDL_SCANCODE_PAGEUP:     return 0x49;
  case SDL_SCANCODE_KP_MINUS:   return 0x4a;
  case SDL_SCANCODE_KP_4:       return 0x4b;
  case SDL_SCANCODE_LEFT:       return 0x4b;
  case SDL_SCANCODE_KP_5:       return 0x4c;
  case SDL_SCANCODE_KP_6:       return 0x4d;
  case SDL_SCANCODE_RIGHT:      return 0x4d;
  case SDL_SCANCODE_KP_PLUS:    return 0x4e;
  case SDL_SCANCODE_KP_1:       return 0x4f;
  case SDL_SCANCODE_END:        return 0x4f;
  case SDL_SCANCODE_KP_2:       return 0x50;
  case SDL_SCANCODE_DOWN:       return 0x50;
  case SDL_SCANCODE_KP_3:       return 0x51;
  case SDL_SCANCODE_PAGEDOWN:   return 0x51;
  case SDL_SCANCODE_KP_0:       return 0x52;
  case SDL_SCANCODE_INSERT:     return 0x52;
  case SDL_SCANCODE_KP_PERIOD:  return 0x53;
  case SDL_SCANCODE_DELETE:     return 0x53;
  case SDL_SCANCODE_F11:        return 0x57;
  case SDL_SCANCODE_F12:        return 0x59; // Matches original BUG: F12=0x59 not 0x58
  case SDL_SCANCODE_PAUSE:      return 0;    // Handled specially
  case SDL_SCANCODE_KP_ENTER:   return 0x1c;
  case SDL_SCANCODE_KP_DIVIDE:  return 0x35;
  default:                      return 0;
  }
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_PumpEvents() - Polls SDL events and updates keyboard/mouse state.
//    Call this every frame instead of relying on a keyboard ISR.
//
///////////////////////////////////////////////////////////////////////////
void
IN_PumpEvents(void)
{
  SDL_Event ev;
  byte k, c;
  int player;

  SD_UpdateTimeCount();

  while (SDL_PollEvent(&ev))
  {
    switch (ev.type)
    {
    case SDL_QUIT:
      Quit(nil);
      break;

    case SDL_KEYDOWN:
      if (ev.key.repeat)
        break;

      if (ev.key.keysym.scancode == SDL_SCANCODE_PAUSE)
      {
        Paused = true;
        break;
      }

      k = INL_SDLScanCodeToDOS(ev.key.keysym.scancode);
      if (k == 0 || k >= NumCodes)
        break;

      LastCode = CurCode;
      CurCode = LastScan = k;
      Keyboard[k] = true;

      if (Latch)
      {
        for (player = 0; player < MaxPlayers; player++)
        {
          if (Controls[player] == ctrl_Keyboard1)
          {
            if (CurCode == KbdDefs[0].button0)
              LatchedButton0[player] = true;
            else if (CurCode == KbdDefs[0].button1)
              LatchedButton1[player] = true;
          }
          else if (Controls[player] == ctrl_Keyboard2)
          {
            if (CurCode == KbdDefs[1].button0)
              LatchedButton0[player] = true;
            else if (CurCode == KbdDefs[1].button1)
              LatchedButton1[player] = true;
          }
        }
      }

      if (k == sc_CapsLock)
      {
        CapsLock ^= true;
      }

      // Determine ASCII value
      c = 0;
      if (Keyboard[sc_LShift] || Keyboard[sc_RShift])
      {
        c = ShiftNames[k];
        if ((c >= 'A') && (c <= 'Z') && CapsLock)
          c += 'a' - 'A';
      }
      else
      {
        c = ASCIINames[k];
        if ((c >= 'a') && (c <= 'z') && CapsLock)
          c -= 'a' - 'A';
      }
      if (c)
        LastASCII = c;

      if (INL_KeyHook)
        INL_KeyHook();
      break;

    case SDL_KEYUP:
      k = INL_SDLScanCodeToDOS(ev.key.keysym.scancode);
      if (k == 0 || k >= NumCodes)
        break;

      Keyboard[k] = false;

      if (INL_KeyHook)
        INL_KeyHook();
      break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////
//
//  INL_GetMouseDelta() - Gets the amount that the mouse has moved from the
//    mouse driver
//
///////////////////////////////////////////////////////////////////////////
static void
INL_GetMouseDelta(int *x,int *y)
{
  SDL_GetRelativeMouseState(x,y);
}

///////////////////////////////////////////////////////////////////////////
//
//  INL_GetMouseButtons() - Gets the status of the mouse buttons from the
//    mouse driver
//
///////////////////////////////////////////////////////////////////////////
static word
INL_GetMouseButtons(void)
{
  word  buttons = 0;
  Uint32 state = SDL_GetMouseState(nil, nil);

  if (state & SDL_BUTTON(SDL_BUTTON_LEFT))
    buttons |= 1;
  if (state & SDL_BUTTON(SDL_BUTTON_RIGHT))
    buttons |= 2;

  return(buttons);
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_GetJoyAbs() - Reads the absolute position of the specified joystick
//
///////////////////////////////////////////////////////////////////////////
void
IN_GetJoyAbs(word joy,word *xp,word *yp)
{
  if (joy < MaxJoys && sdl_joysticks[joy])
  {
    // SDL axis range is -32768..32767, map to 0..MaxJoyValue
    int raw_x = SDL_JoystickGetAxis(sdl_joysticks[joy], 0);
    int raw_y = SDL_JoystickGetAxis(sdl_joysticks[joy], 1);
    *xp = (word)(((long)raw_x + 32768L) * MaxJoyValue / 65535L);
    *yp = (word)(((long)raw_y + 32768L) * MaxJoyValue / 65535L);
  }
  else
  {
    *xp = 0;
    *yp = 0;
  }
}

///////////////////////////////////////////////////////////////////////////
//
//  INL_GetJoyDelta() - Returns the relative movement of the specified
//    joystick (from +/-127, scaled adaptively)
//
///////////////////////////////////////////////////////////////////////////
static void
INL_GetJoyDelta(word joy,int *dx,int *dy,boolean adaptive)
{
  word    x,y;
  longword  time;
  JoystickDef *def;
static  longword  lasttime;

  IN_GetJoyAbs(joy,&x,&y);
  def = JoyDefs + joy;

  if (x < def->threshMinX)
  {
    if (x < def->joyMinX)
      x = def->joyMinX;

    x = -(x - def->threshMinX);
    x *= def->joyMultXL;
    x >>= JoyScaleShift;
    *dx = (x > 127)? -127 : -x;
  }
  else if (x > def->threshMaxX)
  {
    if (x > def->joyMaxX)
      x = def->joyMaxX;

    x = x - def->threshMaxX;
    x *= def->joyMultXH;
    x >>= JoyScaleShift;
    *dx = (x > 127)? 127 : x;
  }
  else
    *dx = 0;

  if (y < def->threshMinY)
  {
    if (y < def->joyMinY)
      y = def->joyMinY;

    y = -(y - def->threshMinY);
    y *= def->joyMultYL;
    y >>= JoyScaleShift;
    *dy = (y > 127)? -127 : -y;
  }
  else if (y > def->threshMaxY)
  {
    if (y > def->joyMaxY)
      y = def->joyMaxY;

    y = y - def->threshMaxY;
    y *= def->joyMultYH;
    y >>= JoyScaleShift;
    *dy = (y > 127)? 127 : y;
  }
  else
    *dy = 0;

  if (adaptive)
  {
    time = (TimeCount - lasttime) / 2;
    if (time)
    {
      if (time > 8)
        time = 8;
      *dx *= time;
      *dy *= time;
    }
  }
  lasttime = TimeCount;
}

///////////////////////////////////////////////////////////////////////////
//
//  INL_GetJoyButtons() - Returns the button status of the specified
//    joystick
//
///////////////////////////////////////////////////////////////////////////
static word
INL_GetJoyButtons(word joy)
{
  word result = 0;

  if (joy < MaxJoys && sdl_joysticks[joy])
  {
    if (SDL_JoystickGetButton(sdl_joysticks[joy], 0))
      result |= 1;
    if (SDL_JoystickGetButton(sdl_joysticks[joy], 1))
      result |= 2;
  }
  return(result);
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_GetJoyButtonsDB() - Returns the de-bounced button status of the
//    specified joystick
//
///////////////////////////////////////////////////////////////////////////
word
IN_GetJoyButtonsDB(word joy)
{
  longword  lasttime;
  word    result1,result2;

  do
  {
    result1 = INL_GetJoyButtons(joy);
    lasttime = TimeCount;
    while (TimeCount == lasttime)
      SD_UpdateTimeCount();
    result2 = INL_GetJoyButtons(joy);
  } while (result1 != result2);
  return(result1);
}

///////////////////////////////////////////////////////////////////////////
//
//  INL_StartKbd() - Sets up keyboard input (no-op in SDL, events are polled)
//
///////////////////////////////////////////////////////////////////////////
static void
INL_StartKbd(void)
{
  INL_KeyHook = 0;  // Clear key hook

  IN_ClearKeysDown();
}

///////////////////////////////////////////////////////////////////////////
//
//  INL_ShutKbd() - Shuts down keyboard input (no-op in SDL)
//
///////////////////////////////////////////////////////////////////////////
static void
INL_ShutKbd(void)
{
}

///////////////////////////////////////////////////////////////////////////
//
//  INL_StartMouse() - Detects and sets up the mouse
//
///////////////////////////////////////////////////////////////////////////
static boolean
INL_StartMouse(void)
{
  // SDL always has mouse support
  SDL_SetRelativeMouseMode(SDL_TRUE);
  return(true);
}

///////////////////////////////////////////////////////////////////////////
//
//  INL_ShutMouse() - Cleans up after the mouse
//
///////////////////////////////////////////////////////////////////////////
static void
INL_ShutMouse(void)
{
  SDL_SetRelativeMouseMode(SDL_FALSE);
}

//
//  INL_SetJoyScale() - Sets up scaling values for the specified joystick
//
static void
INL_SetJoyScale(word joy)
{
  JoystickDef *def;

  def = &JoyDefs[joy];
  def->joyMultXL = JoyScaleMax / (def->threshMinX - def->joyMinX);
  def->joyMultXH = JoyScaleMax / (def->joyMaxX - def->threshMaxX);
  def->joyMultYL = JoyScaleMax / (def->threshMinY - def->joyMinY);
  def->joyMultYH = JoyScaleMax / (def->joyMaxY - def->threshMaxY);
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_SetupJoy() - Sets up thresholding values and calls INL_SetJoyScale()
//    to set up scaling values
//
///////////////////////////////////////////////////////////////////////////
void
IN_SetupJoy(word joy,word minx,word maxx,word miny,word maxy)
{
  word    d,r;
  JoystickDef *def;

  def = &JoyDefs[joy];

  def->joyMinX = minx;
  def->joyMaxX = maxx;
  r = maxx - minx;
  d = r / 5;
  def->threshMinX = ((r / 2) - d) + minx;
  def->threshMaxX = ((r / 2) + d) + minx;

  def->joyMinY = miny;
  def->joyMaxY = maxy;
  r = maxy - miny;
  d = r / 5;
  def->threshMinY = ((r / 2) - d) + miny;
  def->threshMaxY = ((r / 2) + d) + miny;

  INL_SetJoyScale(joy);
}

///////////////////////////////////////////////////////////////////////////
//
//  INL_StartJoy() - Detects & auto-configures the specified joystick
//          The auto-config assumes the joystick is centered
//
///////////////////////////////////////////////////////////////////////////
static boolean
INL_StartJoy(word joy)
{
  int num_joysticks;
  word x, y;

  sdl_joysticks[joy] = nil;

  num_joysticks = SDL_NumJoysticks();
  if ((int)joy >= num_joysticks)
    return(false);

  sdl_joysticks[joy] = SDL_JoystickOpen(joy);
  if (!sdl_joysticks[joy])
    return(false);

  IN_GetJoyAbs(joy,&x,&y);

  if
  (
    ((x == 0) || (x > MaxJoyValue - 10))
  ||  ((y == 0) || (y > MaxJoyValue - 10))
  )
  {
    SDL_JoystickClose(sdl_joysticks[joy]);
    sdl_joysticks[joy] = nil;
    return(false);
  }
  else
  {
    IN_SetupJoy(joy,0,x * 2,0,y * 2);
    return(true);
  }
}

///////////////////////////////////////////////////////////////////////////
//
//  INL_ShutJoy() - Cleans up the joystick stuff
//
///////////////////////////////////////////////////////////////////////////
static void
INL_ShutJoy(word joy)
{
  if (sdl_joysticks[joy])
  {
    SDL_JoystickClose(sdl_joysticks[joy]);
    sdl_joysticks[joy] = nil;
  }
  JoysPresent[joy] = false;
}

//  Public routines

///////////////////////////////////////////////////////////////////////////
//
//  IN_ClearButtonLatch() - Clears the button latch stuff
//
///////////////////////////////////////////////////////////////////////////
void
IN_ClearButtonLatch(void)
{
  int player;

  MouseDownCount = 0;

  for (player = 0; player < MaxPlayers; player++)
  {
    LatchedButton0[player] = LatchedButton1[player] = 0;
  }
}

///////////////////////////////////////////////////////////////////////////
//
//  LatchSndHook() - Hook routine for joystick button latch
//
///////////////////////////////////////////////////////////////////////////
void
LatchSndHook(void)
{
  int player;
  ControlType ctrl;
  word buttons;

  for (player = 0; player < MaxPlayers; player++)
  {
    ctrl = Controls[player];

    if (ctrl == ctrl_Joystick1 || ctrl == ctrl_Joystick2)
    {
      buttons = INL_GetJoyButtons(ctrl - ctrl_Joystick1);

      if (buttons & 1)
        LatchedButton0[player] = true;
      if (buttons & 2)
        LatchedButton1[player] = true;
    }
  }
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_LatchButtons() - Enables or disables button latch
//
///////////////////////////////////////////////////////////////////////////
void IN_LatchButtons(boolean enabled)
{
  if (enabled)
  {
    Latch = false;
    IN_ClearButtonLatch();
  }

  Latch = enabled;
  SD_SetUserHook(Latch ? LatchSndHook : NULL);
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_Startup() - Starts up the Input Mgr
//
///////////////////////////////////////////////////////////////////////////
void
IN_Startup(void)
{
  boolean checkjoys,checkmouse;
  word  i;

  if (IN_Started)
    return;

  checkjoys = true;
  checkmouse = true;
  for (i = 1;i < _argc;i++)
  {
    switch (US_CheckParm(_argv[i],ParmStrings))
    {
    case 0:
      checkjoys = false;
      break;
    case 1:
      checkmouse = false;
      break;
    }
  }

  INL_StartKbd();
  MousePresent = checkmouse? INL_StartMouse() : false;

  if (checkjoys)
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);

  for (i = 0;i < MaxJoys;i++)
    JoysPresent[i] = checkjoys? INL_StartJoy(i) : false;

  IN_Started = true;
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_Default() - Sets up default conditions for the Input Mgr
//
///////////////////////////////////////////////////////////////////////////
void
IN_Default(boolean gotit,ControlType in)
{
  if
  (
    (!gotit)
  ||  ((in == ctrl_Joystick1) && !JoysPresent[0])
  ||  ((in == ctrl_Joystick2) && !JoysPresent[1])
  ||  ((in == ctrl_Mouse) && !MousePresent)
  )
    in = ctrl_Keyboard1;
  IN_SetControlType(0,in);
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_Shutdown() - Shuts down the Input Mgr
//
///////////////////////////////////////////////////////////////////////////
void
IN_Shutdown(void)
{
  word  i;

  if (!IN_Started)
    return;

  INL_ShutMouse();
  for (i = 0;i < MaxJoys;i++)
    INL_ShutJoy(i);
  INL_ShutKbd();

  IN_Started = false;
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_SetKeyHook() - Sets the routine that gets called by IN_PumpEvents()
//      everytime a real make/break code gets hit
//
///////////////////////////////////////////////////////////////////////////
void
IN_SetKeyHook(void (*hook)())
{
  INL_KeyHook = hook;
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_ClearKeyDown() - Clears the keyboard array
//
///////////////////////////////////////////////////////////////////////////
void
IN_ClearKeysDown(void)
{
  int i;

  LastScan = sc_None;
  LastASCII = key_None;
  for (i = 0;i < NumCodes;i++)
    Keyboard[i] = false;
}

///////////////////////////////////////////////////////////////////////////
//
//  INL_AdjustCursor() - Internal routine of common code from IN_ReadCursor()
//
///////////////////////////////////////////////////////////////////////////
static void
INL_AdjustCursor(CursorInfo *info,word buttons,int dx,int dy)
{
  if (buttons & (1 << 0))
    info->button0 = true;
  if (buttons & (1 << 1))
    info->button1 = true;

  info->x += dx;
  info->y += dy;
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_ReadCursor() - Reads the input devices and fills in the cursor info
//    struct
//
///////////////////////////////////////////////////////////////////////////
void
IN_ReadCursor(CursorInfo *info)
{
  word  i,
    player,
      buttons;
  int   dx,dy;

  info->x = info->y = 0;
  info->button0 = info->button1 = false;

  if (MousePresent)
  {
    buttons = INL_GetMouseButtons();
    INL_GetMouseDelta(&dx,&dy);
    INL_AdjustCursor(info,buttons,dx,dy);
  }

  for (i = 0;i < MaxJoys;i++)
  {
    if (!JoysPresent[i])
      continue;

    for (player = 0;player < MaxPlayers; player++)
    {
      if (Controls[player] == ctrl_Joystick1+i)
        goto joyok;
    }
    continue;

joyok:
    buttons = INL_GetJoyButtons(i);
    INL_GetJoyDelta(i,&dx,&dy,true);
    dx /= 64;
    dy /= 64;
    INL_AdjustCursor(info,buttons,dx,dy);
  }
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_ReadControl() - Reads the device associated with the specified
//    player and fills in the control info struct
//
///////////////////////////////////////////////////////////////////////////
void
IN_ReadControl(int player,ControlInfo *info)
{
      boolean   realdelta;
      byte    dbyte;
      word    buttons;
      int     dx,dy;
      Motion    mx,my;
      ControlType type;
register  KeyboardDef *def;

  dx = dy = 0;
  mx = my = motion_None;
  buttons = 0;

  if (DemoMode == demo_Playback)
  {
    dbyte = DemoBuffer[DemoOffset + 1];
    my = (dbyte & 3) - 1;
    mx = ((dbyte >> 2) & 3) - 1;
    buttons = (dbyte >> 4) & 3;

    if (!(DemoBuffer[DemoOffset] -= 1))
    {
      DemoOffset += 2;
      if (DemoOffset >= DemoSize)
        DemoMode = demo_PlayDone;
    }
    realdelta = 0;
  }
  else if (DemoMode == demo_PlayDone)
    Quit("Demo playback exceeded");
  else
  {
    switch (type = Controls[player])
    {
    case ctrl_Keyboard1:
    case ctrl_Keyboard2:
      def = &KbdDefs[type - ctrl_Keyboard];

      if (Keyboard[def->upleft])
        mx = motion_Left,my = motion_Up;
      else if (Keyboard[def->upright])
        mx = motion_Right,my = motion_Up;
      else if (Keyboard[def->downleft])
        mx = motion_Left,my = motion_Down;
      else if (Keyboard[def->downright])
        mx = motion_Right,my = motion_Down;

      if (Keyboard[def->up])
        my = motion_Up;
      else if (Keyboard[def->down])
        my = motion_Down;

      if (Keyboard[def->left])
        mx = motion_Left;
      else if (Keyboard[def->right])
        mx = motion_Right;

      if (Keyboard[def->button0])
        buttons += 1 << 0;
      if (Keyboard[def->button1])
        buttons += 1 << 1;
      realdelta = false;
      break;
    case ctrl_Joystick1:
    case ctrl_Joystick2:
      INL_GetJoyDelta(type - ctrl_Joystick,&dx,&dy,false);
      buttons = INL_GetJoyButtons(type - ctrl_Joystick);
      realdelta = true;
      break;
    case ctrl_Mouse:
      INL_GetMouseDelta(&dx,&dy);
      buttons = INL_GetMouseButtons();
      realdelta = true;
      break;
    }
  }

  if (realdelta)
  {
    mx = (dx < 0)? motion_Left : ((dx > 0)? motion_Right : motion_None);
    my = (dy < 0)? motion_Up : ((dy > 0)? motion_Down : motion_None);
  }
  else
  {
    dx = mx * 127;
    dy = my * 127;
  }

  info->x = dx;
  info->xaxis = mx;
  info->y = dy;
  info->yaxis = my;
  info->button0 = buttons & (1 << 0);
  info->button1 = buttons & (1 << 1);
  info->dir = DirTable[((my + 1) * 3) + (mx + 1)];

  if (DemoMode == demo_Record)
  {
    // Pack the control info into a byte
    dbyte = (buttons << 4) | ((mx + 1) << 2) | (my + 1);

    if
    (
      (DemoBuffer[DemoOffset + 1] == dbyte)
    &&  (DemoBuffer[DemoOffset] < 255)
    )
      DemoBuffer[DemoOffset]++;
    else
    {
      if (DemoOffset || DemoBuffer[DemoOffset])
        DemoOffset += 2;

      if (DemoOffset >= DemoSize)
        Quit("Demo buffer overflow");

      DemoBuffer[DemoOffset] = 1;
      DemoBuffer[DemoOffset + 1] = dbyte;
    }
  }
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_SetControlType() - Sets the control type to be used by the specified
//    player
//
///////////////////////////////////////////////////////////////////////////
void
IN_SetControlType(int player,ControlType type)
{
  // DEBUG - check that requested type is present?
  Controls[player] = type;
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_StartDemoRecord() - Starts the demo recording, using a buffer the
//    size passed. Returns if the buffer allocation was successful
//
///////////////////////////////////////////////////////////////////////////
boolean
IN_StartDemoRecord(word bufsize)
{
  if (!bufsize)
    return(false);

  MM_GetPtr((memptr *)&DemoBuffer,bufsize);
  DemoMode = demo_Record;
  DemoSize = bufsize & ~1;
  DemoOffset = 0;
  DemoBuffer[0] = DemoBuffer[1] = 0;

  return(true);
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_StartDemoPlayback() - Plays back the demo pointed to of the given size
//
///////////////////////////////////////////////////////////////////////////
void
IN_StartDemoPlayback(byte _seg *buffer,word bufsize)
{
  DemoBuffer = buffer;
  DemoMode = demo_Playback;
  DemoSize = bufsize & ~1;
  DemoOffset = 0;
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_StopDemo() - Turns off demo mode
//
///////////////////////////////////////////////////////////////////////////
void
IN_StopDemo(void)
{
  if ((DemoMode == demo_Record) && DemoOffset)
    DemoOffset += 2;

  DemoMode = demo_Off;
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_FreeDemoBuffer() - Frees the demo buffer, if it's been allocated
//
///////////////////////////////////////////////////////////////////////////
void
IN_FreeDemoBuffer(void)
{
  if (DemoBuffer)
    MM_FreePtr((memptr *)&DemoBuffer);
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_GetScanName() - Returns a string containing the name of the
//    specified scan code
//
///////////////////////////////////////////////////////////////////////////
byte *
IN_GetScanName(ScanCode scan)
{
  byte    **p;
  ScanCode  far *s;

  for (s = ExtScanCodes,p = ExtScanNames;*s;p++,s++)
    if (*s == scan)
      return(*p);

  return(ScanNames[scan]);
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_WaitForKey() - Waits for a scan code, then clears LastScan and
//    returns the scan code
//
///////////////////////////////////////////////////////////////////////////
ScanCode
IN_WaitForKey(void)
{
  ScanCode  result;

  while (!(result = LastScan))
    IN_PumpEvents();
  LastScan = 0;
  return(result);
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_WaitForASCII() - Waits for an ASCII char, then clears LastASCII and
//    returns the ASCII value
//
///////////////////////////////////////////////////////////////////////////
char
IN_WaitForASCII(void)
{
  char    result;

  while (!(result = LastASCII))
    IN_PumpEvents();
  LastASCII = '\0';
  return(result);
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_AckBack() - Waits for either an ASCII keypress or a button press
//
///////////////////////////////////////////////////////////////////////////
void
IN_AckBack(void)
{
  word  i;

  while (!LastScan)
  {
    IN_PumpEvents();

    if (MousePresent)
    {
      if (INL_GetMouseButtons())
      {
        while (INL_GetMouseButtons())
          IN_PumpEvents();
        return;
      }
    }

    for (i = 0;i < MaxJoys;i++)
    {
      if (JoysPresent[i])
      {
        if (IN_GetJoyButtonsDB(i))
        {
          while (IN_GetJoyButtonsDB(i))
            ;
          return;
        }
      }
    }
  }

  IN_ClearKey(LastScan);
  LastScan = sc_None;
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_Ack() - Clears user input & then calls IN_AckBack()
//
///////////////////////////////////////////////////////////////////////////
void
IN_Ack(void)
{
  word  i;

  IN_ClearKey(LastScan);
  LastScan = sc_None;

  if (MousePresent)
    while (INL_GetMouseButtons())
      IN_PumpEvents();
  for (i = 0;i < MaxJoys;i++)
    if (JoysPresent[i])
      while (IN_GetJoyButtonsDB(i))
        ;

  IN_AckBack();
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_IsUserInput() - Returns true if a key has been pressed or a button
//    is down
//
///////////////////////////////////////////////////////////////////////////
boolean
IN_IsUserInput(void)
{
  boolean result;
  word  i;

  result = LastScan;

  if (MousePresent)
    if (INL_GetMouseButtons())
      result = true;

  for (i = 0;i < MaxJoys;i++)
    if (JoysPresent[i])
      if (INL_GetJoyButtons(i))
        result = true;

  return(result);
}

///////////////////////////////////////////////////////////////////////////
//
//  IN_UserInput() - Waits for the specified delay time (in ticks) or the
//    user pressing a key or a mouse button. If the clear flag is set, it
//    then either clears the key or waits for the user to let the mouse
//    button up.
//
///////////////////////////////////////////////////////////////////////////
boolean
IN_UserInput(longword delay,boolean clear)
{
  longword  lasttime;

  lasttime = TimeCount;
  do
  {
    IN_PumpEvents();
    if (IN_IsUserInput())
    {
      if (clear)
        IN_AckBack();
      return(true);
    }
  } while (TimeCount - lasttime < delay);
  return(false);
}
