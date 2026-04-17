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

// ID_VW.C - SDL2 video backend
//
// Emulates EGA 4-bitplane graphics using a software framebuffer.
// All internal drawing is in EGA bitplane format; conversion to
// linear RGB happens only at presentation time.

#include "ID_HEADS.H"
#include <SDL.h>
#ifdef SDL_PORT
#include "ID_TEXTSCR.h"
#endif

extern int _argc;
extern char **_argv;

/*
=============================================================================

                         LOCAL CONSTANTS

=============================================================================
*/

#define VIEWWIDTH       40

#define PIXTOBLOCK      4       // 16 pixels to an update block

#define BUFFWIDTH       50
#define BUFFHEIGHT      32

#define SCREEN_SCALE    3
#define SCREEN_W        320
#define SCREEN_H        200

// EGA VRAM size per plane (original hardware had 64KB per plane)
#define EGA_VRAM_SIZE   0x10000u

// On DOS, unsigned was 16-bit so VRAM address arithmetic naturally wrapped
// at 64KB.  On modern platforms unsigned is 32-bit, so we must wrap
// explicitly to emulate the circular EGA VRAM.
#define VRAM_WRAP(ofs)  ((ofs) & (EGA_VRAM_SIZE - 1))

/*
=============================================================================

                         GLOBAL VARIABLES

=============================================================================
*/

cardtype    videocard;      // set by VW_Startup
grtype      grmode;         // CGAgr, EGAgr, VGAgr

unsigned    bufferofs;      // hidden area to draw to before displaying
unsigned    displayofs;     // origin of the visable screen
unsigned    panx,pany;      // panning adjustments inside port in pixels
unsigned    pansx,pansy;    // panning adjustments inside port in screen
                            // block limited pixel values (ie 0/8 for ega x)
unsigned    panadjust;      // panx/pany adjusted by screen resolution

unsigned    screenseg;      // kept for compatibility (unused in SDL port)
unsigned    linewidth;
unsigned    ylookup[VIRTUALHEIGHT];

unsigned    fontnumber;     // 0 based font number for drawing

boolean     screenfaded;
boolean nopan;

pictabletype    _seg *pictable;
pictabletype    _seg *picmtable;
spritetabletype _seg *spritetable;

int         bordercolor;
boolean     latchpel;

int         px,py;
byte        pdrawmode,fontcolor;

unsigned    bufferwidth,bufferheight,screenspot;

/*
=============================================================================

                         LOCAL VARIABLES

=============================================================================
*/

static void VWL_DrawCursor (void);
static void VWL_EraseCursor (void);
static void VWL_UpdateScreenBlocks (void);

static int      cursorvisible;
static int      cursornumber,cursorwidth,cursorheight,cursorx,cursory;
static memptr   cursorsave;
static unsigned cursorspot;

/*
=============================================================================

                         SDL2 VIDEO STATE

=============================================================================
*/

static SDL_Window *sdl_window;
static SDL_Renderer *sdl_renderer;
static SDL_Texture *sdl_texture;

// Software EGA framebuffer: 4 bitplanes
static uint8_t screenbuffer[4][EGA_VRAM_SIZE];

// Active palette: 16 EGA color indices mapped to RGB
static uint8_t active_palette[16][3];

// Standard 6-bit EGA 64-color palette
static const uint8_t ega_64colors[64][3] = {
    {0x00,0x00,0x00}, {0x00,0x00,0xAA}, {0x00,0xAA,0x00}, {0x00,0xAA,0xAA},
    {0xAA,0x00,0x00}, {0xAA,0x00,0xAA}, {0xAA,0x55,0x00}, {0xAA,0xAA,0xAA},
    {0x00,0x00,0x55}, {0x00,0x00,0xFF}, {0x00,0xAA,0x55}, {0x00,0xAA,0xFF},
    {0xAA,0x00,0x55}, {0xAA,0x00,0xFF}, {0xAA,0x55,0x55}, {0xAA,0xAA,0xFF},
    {0x00,0x55,0x00}, {0x00,0x55,0xAA}, {0x00,0xFF,0x00}, {0x00,0xFF,0xAA},
    {0xAA,0x55,0x00}, {0xAA,0x55,0xAA}, {0xAA,0xFF,0x00}, {0xAA,0xFF,0xAA},
    {0x00,0x55,0x55}, {0x00,0x55,0xFF}, {0x00,0xFF,0x55}, {0x00,0xFF,0xFF},
    {0xAA,0x55,0x55}, {0xAA,0x55,0xFF}, {0xAA,0xFF,0x55}, {0xAA,0xFF,0xFF},
    {0x55,0x00,0x00}, {0x55,0x00,0xAA}, {0x55,0xAA,0x00}, {0x55,0xAA,0xAA},
    {0xFF,0x00,0x00}, {0xFF,0x00,0xAA}, {0xFF,0xAA,0x00}, {0xFF,0xAA,0xAA},
    {0x55,0x00,0x55}, {0x55,0x00,0xFF}, {0x55,0xAA,0x55}, {0x55,0xAA,0xFF},
    {0xFF,0x00,0x55}, {0xFF,0x00,0xFF}, {0xFF,0xAA,0x55}, {0xFF,0xAA,0xFF},
    {0x55,0x55,0x00}, {0x55,0x55,0xAA}, {0x55,0xFF,0x00}, {0x55,0xFF,0xAA},
    {0xFF,0x55,0x00}, {0xFF,0x55,0xAA}, {0xFF,0xFF,0x00}, {0xFF,0xFF,0xAA},
    {0x55,0x55,0x55}, {0x55,0x55,0xFF}, {0x55,0xFF,0x55}, {0x55,0xFF,0xFF},
    {0xFF,0x55,0x55}, {0xFF,0x55,0xFF}, {0xFF,0xFF,0x55}, {0xFF,0xFF,0xFF},
};

// Shift tables for font rendering (replaces the ASM shift data)
static unsigned shifttable_data[8][256];
unsigned *shifttabletable[8];

// Font rendering buffer
static uint8_t databuffer[BUFFWIDTH*BUFFHEIGHT];
static unsigned bufferbyte;
static unsigned bufferbit;

// Pan offset for pixel panning (0-7)
static unsigned pelpan_offset;

/*
=============================================================================

                     SHIFT TABLE INITIALIZATION

=============================================================================
*/

static void VWL_InitShiftTables(void)
{
    int table, val;

    for (table = 0; table < 8; table++)
    {
        shifttabletable[table] = shifttable_data[table];

        for (val = 0; val < 256; val++)
        {
            if (table == 0)
            {
                shifttable_data[table][val] = val;
            }
            else
            {
                unsigned lo = (val >> table) & 0xFF;
                unsigned hi = (val << (8 - table)) & 0xFF;
                shifttable_data[table][val] = lo | (hi << 8);
            }
        }
    }
}

/*
=============================================================================

                         PALETTE MANAGEMENT

=============================================================================
*/

static void VWL_SetActivePalette(char *palette)
{
    int i;
    for (i = 0; i < 16; i++)
    {
        unsigned idx = (unsigned)(unsigned char)palette[i];
        if (idx < 64)
        {
            active_palette[i][0] = ega_64colors[idx][0];
            active_palette[i][1] = ega_64colors[idx][1];
            active_palette[i][2] = ega_64colors[idx][2];
        }
    }
}

/*
=============================================================================

                     PRESENTATION (EGA -> SDL)

=============================================================================
*/

void VW_Present(void)
{
    uint8_t *pixels;
    int pitch;
    int x, y;

    if (!sdl_texture)
        return;

    if (SDL_LockTexture(sdl_texture, NULL, (void**)&pixels, &pitch) != 0)
        return;

    for (y = 0; y < SCREEN_H; y++)
    {
        for (x = 0; x < SCREEN_W; x++)
        {
            int srcx = x + pelpan_offset;
            int srcbyte = (srcx >> 3);
            int srcbit = 7 - (srcx & 7);
            unsigned planeofs = VRAM_WRAP(displayofs + ylookup[y] + srcbyte);

            unsigned color = ((screenbuffer[0][planeofs] >> srcbit) & 1)
                      | (((screenbuffer[1][planeofs] >> srcbit) & 1) << 1)
                      | (((screenbuffer[2][planeofs] >> srcbit) & 1) << 2)
                      | (((screenbuffer[3][planeofs] >> srcbit) & 1) << 3);

            {
                uint8_t *p = pixels + y * pitch + x * 4;
                p[0] = active_palette[color][2]; // B
                p[1] = active_palette[color][1]; // G
                p[2] = active_palette[color][0]; // R
                p[3] = 0xFF;                     // A
            }
        }
    }

    SDL_UnlockTexture(sdl_texture);
    SDL_RenderClear(sdl_renderer);
    SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
    SDL_RenderPresent(sdl_renderer);
}

//===========================================================================

/*
=======================
=
= VW_Startup
=
=======================
*/

static  char *ParmStrings[] = {"HIDDENCARD","LATCHPEL",""};

void    VW_Startup (void)
{
    int i,n;

    videocard = 0;

    for (i = 1;i < _argc;i++)
    {
        n = US_CheckParm(_argv[i],ParmStrings);
        if (n == 0)
        {
            videocard = EGAcard;
        }
        else if (n == 1)
        {
            latchpel = true;
        }
    }

    if (!videocard)
        videocard = VW_VideoID ();

    grmode = EGAGR;

    // SDL window/renderer/texture creation is deferred to VWL_SetupSDLWindow()
    // which is called from VW_SetScreenMode(). This allows the text screen to
    // keep using its window until it's done.

    // Clear framebuffer
    memset(screenbuffer, 0, sizeof(screenbuffer));

    // Initialize shift tables for font rendering
    VWL_InitShiftTables();

    // Set default draw mode and font color
    pdrawmode = 0x18; // XOR mode
    fontcolor = 15;

    // Initialize palette to standard EGA default
    {
        static const uint8_t default_ega[16] = {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
            0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
        };
        for (i = 0; i < 16; i++)
        {
            active_palette[i][0] = ega_64colors[default_ega[i]][0];
            active_palette[i][1] = ega_64colors[default_ega[i]][1];
            active_palette[i][2] = ega_64colors[default_ega[i]][2];
        }
    }

    cursorvisible = 0;
}

//===========================================================================

/*
=========================
=
= VWL_SetupSDLWindow - Creates (or takes over) the SDL window, renderer,
=                       and texture for graphics mode.
=
=========================
*/

static void VWL_SetupSDLWindow(void)
{
    if (sdl_window && sdl_renderer && sdl_texture)
        return; // already fully set up

    // Initialize SDL video
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
    {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
            Quit("SDL video init failed!");
    }

    // Try to reuse the text screen window (avoids creating a second window)
#ifdef SDL_PORT
    sdl_window = (SDL_Window *)TXT_TransferWindow();
    // Shut down the text-mode renderer/texture now that the window has been
    // transferred.  TXT_Shutdown() will NOT destroy the window because
    // TXT_TransferWindow() already cleared the internal pointer.
    TXT_Shutdown();
#endif

    if (!sdl_window)
    {
        sdl_window = SDL_CreateWindow("Bio Menace",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            SCREEN_W * SCREEN_SCALE, SCREEN_H * SCREEN_SCALE,
            SDL_WINDOW_SHOWN);
    }
    else
    {
        // Resize the transferred text-mode window for graphics mode
        SDL_SetWindowSize(sdl_window, SCREEN_W * SCREEN_SCALE, SCREEN_H * SCREEN_SCALE);
        SDL_SetWindowPosition(sdl_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    if (!sdl_window)
        Quit("SDL_CreateWindow failed!");

    sdl_renderer = SDL_CreateRenderer(sdl_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdl_renderer)
        sdl_renderer = SDL_CreateRenderer(sdl_window, -1, 0);
    if (!sdl_renderer)
        Quit("SDL_CreateRenderer failed!");

    SDL_RenderSetLogicalSize(sdl_renderer, SCREEN_W, SCREEN_H);

    sdl_texture = SDL_CreateTexture(sdl_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_W, SCREEN_H);
    if (!sdl_texture)
        Quit("SDL_CreateTexture failed!");
}

//===========================================================================

/*
=======================
=
= VW_Shutdown
=
=======================
*/

void    VW_Shutdown (void)
{
#ifdef SDL_PORT
    // In case Quit() is called before the text-mode window was transferred
    // to graphics mode, make sure the text subsystem is cleaned up.
    TXT_Shutdown();
#endif

    if (sdl_texture)
    {
        SDL_DestroyTexture(sdl_texture);
        sdl_texture = NULL;
    }
    if (sdl_renderer)
    {
        SDL_DestroyRenderer(sdl_renderer);
        sdl_renderer = NULL;
    }
    if (sdl_window)
    {
        SDL_DestroyWindow(sdl_window);
        sdl_window = NULL;
    }
}

//===========================================================================

/*
========================
=
= VW_SetScreenMode
=
========================
*/

void VW_SetScreenMode (int grmode)
{
    switch (grmode)
    {
        case TEXTGR:
            break;
        case CGAGR:
        case EGAGR:
            VWL_SetupSDLWindow();
            memset(screenbuffer, 0, sizeof(screenbuffer));
            screenseg = 0xa000;
            break;
    }
    VW_SetLineWidth(SCREENWIDTH);
}

/*
=============================================================================

                            SCREEN FADES

=============================================================================
*/

char colors[7][17]=
{{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,0},
 {0,0,0,0,0,0,0,0,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0},
 {0,1,2,3,4,5,6,7,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0},
 {0,1,2,3,4,5,6,7,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0},
 {0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f}};


void VW_ColorBorder (int color)
{
    bordercolor = color;
}

void VW_SetPalette(byte *palette)
{
    byte    p;
    word    i;

    for (i = 0;i < 15;i++)
    {
        p = palette[i];
        colors[0][i] = 0;
        colors[1][i] = (p > 0x10)? (p & 0x0f) : 0;
        colors[2][i] = (p > 0x10)? p : 0;
        colors[3][i] = p;
        colors[4][i] = (p > 0x10)? 0x1f : p;
        colors[5][i] = 0x1f;
    }
}

void VW_SetDefaultColors(void)
{
    colors[3][16] = bordercolor;
    VWL_SetActivePalette(colors[3]);
    screenfaded = false;
    VW_Present();
}


void VW_FadeOut(void)
{
    int i;

    for (i=3;i>=0;i--)
    {
        colors[i][16] = bordercolor;
        VWL_SetActivePalette(colors[i]);
        VW_Present();
        SDL_Delay(6 * 1000 / 70);
    }
    screenfaded = true;
}


void VW_FadeIn(void)
{
    int i;

    for (i=0;i<4;i++)
    {
        colors[i][16] = bordercolor;
        VWL_SetActivePalette(colors[i]);
        VW_Present();
        SDL_Delay(6 * 1000 / 70);
    }
    screenfaded = false;
}

void VW_FadeUp(void)
{
    int i;

    for (i=3;i<6;i++)
    {
        colors[i][16] = bordercolor;
        VWL_SetActivePalette(colors[i]);
        VW_Present();
        SDL_Delay(6 * 1000 / 70);
    }
    screenfaded = true;
}

void VW_FadeDown(void)
{
    int i;

    for (i=5;i>2;i--)
    {
        colors[i][16] = bordercolor;
        VWL_SetActivePalette(colors[i]);
        VW_Present();
        SDL_Delay(6 * 1000 / 70);
    }
    screenfaded = false;
}


/*
========================
=
= VW_SetAtrReg
=
= No-op in SDL port
=
========================
*/

void VW_SetAtrReg (int reg, int value)
{
    (void)reg;
    (void)value;
}



//===========================================================================

/*
====================
=
= VW_SetLineWidth
=
= Must be an even number of bytes
=
====================
*/

void VW_SetLineWidth (int width)
{
    int i,offset;

    linewidth = width;

    offset = 0;

    for (i=0;i<VIRTUALHEIGHT;i++)
    {
        ylookup[i]=offset;
        offset += width;
    }
}


//===========================================================================

/*
====================
=
= VW_SetSplitScreen
=
= No-op in SDL port
=
====================
*/

void VW_SetSplitScreen (int linenum)
{
    (void)linenum;
}

//===========================================================================

/*
====================
=
= VW_ClearVideo
=
====================
*/

void    VW_ClearVideo (int color)
{
    int plane;

    VW_WaitVBL(1);

    for (plane = 0; plane < 4; plane++)
    {
        uint8_t fill = (color & (1 << plane)) ? 0xFF : 0x00;
        memset(screenbuffer[plane], fill, sizeof(screenbuffer[plane]));
    }
}

//===========================================================================

/*
====================
=
= VW_WaitVBL
=
====================
*/

void VW_WaitVBL (int number)
{
    if (number > 0)
        SDL_Delay(number * 1000 / 70);
}

//===========================================================================

/*
====================
=
= VW_SetScreen
=
====================
*/

void VW_SetScreen (unsigned crtc, unsigned pel)
{
    displayofs = crtc;
    pelpan_offset = pel;
    VW_Present();
}

//===========================================================================

/*
====================
=
= VW_VideoID
=
====================
*/

cardtype VW_VideoID (void)
{
    return VGAcard;
}

//===========================================================================

/*
====================
=
= VW_Plot
=
====================
*/

void VW_Plot(unsigned x, unsigned y, unsigned color)
{
    unsigned byteofs = VRAM_WRAP(bufferofs + ylookup[y] + (x >> 3));
    unsigned bitmask = 0x80 >> (x & 7);
    int plane;

    for (plane = 0; plane < 4; plane++)
    {
        if (color & (1 << plane))
            screenbuffer[plane][byteofs] |= bitmask;
        else
            screenbuffer[plane][byteofs] &= ~bitmask;
    }
}


//===========================================================================

/*
====================
=
= VW_Vlin
=
====================
*/

void VW_Vlin(unsigned yl, unsigned yh, unsigned x, unsigned color)
{
    unsigned byteofs;
    unsigned bitmask;
    unsigned y;
    int plane;

    bitmask = 0x80 >> (x & 7);
    byteofs = bufferofs + ylookup[yl] + (x >> 3);

    for (y = yl; y <= yh; y++)
    {
        for (plane = 0; plane < 4; plane++)
        {
            if (color & (1 << plane))
                screenbuffer[plane][VRAM_WRAP(byteofs)] |= bitmask;
            else
                screenbuffer[plane][VRAM_WRAP(byteofs)] &= ~bitmask;
        }
        byteofs += linewidth;
    }
}


//===========================================================================

/*
====================
=
= VW_Hlin
=
====================
*/

#if GRMODE == EGAGR

unsigned char leftmask[8] = {0xff,0x7f,0x3f,0x1f,0xf,7,3,1};
unsigned char rightmask[8] = {0x80,0xc0,0xe0,0xf0,0xf8,0xfc,0xfe,0xff};

void VW_Hlin(unsigned xl, unsigned xh, unsigned y, unsigned color)
{
    unsigned xlb,xhb,mid;
    uint8_t maskleft,maskright;
    unsigned dest;
    int plane;

    xlb = xl/8;
    xhb = xh/8;

    maskleft = leftmask[xl&7];
    maskright = rightmask[xh&7];

    dest = VRAM_WRAP(bufferofs + ylookup[y] + xlb);

    if (xlb == xhb)
    {
        uint8_t mask = maskleft & maskright;
        for (plane = 0; plane < 4; plane++)
        {
            if (color & (1 << plane))
                screenbuffer[plane][dest] |= mask;
            else
                screenbuffer[plane][dest] &= ~mask;
        }
        return;
    }

    // Left edge
    for (plane = 0; plane < 4; plane++)
    {
        if (color & (1 << plane))
            screenbuffer[plane][dest] |= maskleft;
        else
            screenbuffer[plane][dest] &= ~maskleft;
    }

    // Middle full bytes
    mid = xhb - xlb - 1;
    if (mid > 0)
    {
        unsigned pos = dest + 1;
        unsigned i;
        for (i = 0; i < mid; i++, pos++)
        {
            unsigned wpos = VRAM_WRAP(pos);
            for (plane = 0; plane < 4; plane++)
            {
                if (color & (1 << plane))
                    screenbuffer[plane][wpos] = 0xFF;
                else
                    screenbuffer[plane][wpos] = 0x00;
            }
        }
    }

    // Right edge
    {
        unsigned rpos = VRAM_WRAP(dest + (xhb - xlb));
        for (plane = 0; plane < 4; plane++)
        {
            if (color & (1 << plane))
                screenbuffer[plane][rpos] |= maskright;
            else
                screenbuffer[plane][rpos] &= ~maskright;
        }
    }
}

#endif

#if GRMODE == CGAGR

unsigned char pixmask[4] = {0xc0,0x30,0x0c,0x03};
unsigned char leftmask[4] = {0xff,0x3f,0x0f,0x03};
unsigned char rightmask[4] = {0xc0,0xf0,0xfc,0xff};
unsigned char colorbyte[4] = {0,0x55,0xaa,0xff};

void VW_Hlin(unsigned xl, unsigned xh, unsigned y, unsigned color)
{
    (void)xl; (void)xh; (void)y; (void)color;
}

#endif


/*
==================
=
= VW_Bar
=
==================
*/

void VW_Bar (unsigned x, unsigned y, unsigned width, unsigned height,
    unsigned color)
{
    unsigned xh = x+width-1;

    while (height--)
        VW_Hlin (x,xh,y++,color);
}

//===========================================================================

/*
====================
=
= VW_DrawTile8
=
====================
*/

void VW_DrawTile8(unsigned x, unsigned y, unsigned tile)
{
    uint8_t *source;
    unsigned dest;
    int plane, row;

    source = (uint8_t *)grsegs[STARTTILE8];
    source += tile * 32;

    dest = bufferofs + x + ylookup[y];

    for (plane = 0; plane < 4; plane++)
    {
        unsigned pos = dest;
        for (row = 0; row < 8; row++)
        {
            screenbuffer[plane][VRAM_WRAP(pos)] = *source;
            source++;
            pos += linewidth;
        }
    }
}


//===========================================================================

/*
====================
=
= VW_MaskBlock
=
====================
*/

void VW_MaskBlock(memptr segm, unsigned ofs, unsigned dest,
    unsigned wide, unsigned height, unsigned planesize)
{
    uint8_t *source = (uint8_t *)segm;
    uint8_t *mask = source + ofs;
    uint8_t *data = mask + planesize;
    int plane;
    unsigned row, col;

    if (wide == 0 || height == 0)
        return;

    for (plane = 0; plane < 4; plane++)
    {
        uint8_t *mp = mask;
        uint8_t *dp = data;
        unsigned screenofs = dest;

        for (row = 0; row < height; row++)
        {
            for (col = 0; col < wide; col++)
            {
                unsigned wofs = VRAM_WRAP(screenofs + col);
                screenbuffer[plane][wofs] =
                    (screenbuffer[plane][wofs] & mp[col]) | dp[col];
            }
            mp += wide;
            dp += wide;
            screenofs += linewidth;
        }

        data += planesize;
    }
}


//===========================================================================

/*
====================
=
= VW_InverseMask
=
====================
*/

void VW_InverseMask(memptr segm, unsigned ofs, unsigned dest,
    unsigned wide, unsigned height)
{
    uint8_t *source = (uint8_t *)segm + ofs;
    unsigned row, col;
    int plane;

    for (row = 0; row < height; row++)
    {
        for (col = 0; col < wide; col++)
        {
            unsigned pos = VRAM_WRAP(dest + col);
            uint8_t val = ~(*source);
            source++;
            for (plane = 0; plane < 4; plane++)
                screenbuffer[plane][pos] |= val;
        }
        dest += linewidth;
    }
}


//===========================================================================

/*
====================
=
= VW_ScreenToScreen
=
====================
*/

void VW_ScreenToScreen(unsigned source, unsigned dest, unsigned wide, unsigned height)
{
    unsigned row, col;
    int plane;

    for (plane = 0; plane < 4; plane++)
    {
        unsigned sofs = source;
        unsigned dofs = dest;

        for (row = 0; row < height; row++)
        {
            for (col = 0; col < wide; col++)
            {
                screenbuffer[plane][VRAM_WRAP(dofs + col)] = screenbuffer[plane][VRAM_WRAP(sofs + col)];
            }
            sofs += linewidth;
            dofs += linewidth;
        }
    }
}


//===========================================================================

/*
====================
=
= VW_MemToScreen
=
====================
*/

void VW_MemToScreen(memptr source, unsigned dest, unsigned width, unsigned height)
{
    uint8_t *src = (uint8_t *)source;
    int plane;
    unsigned row, col;

    for (plane = 0; plane < 4; plane++)
    {
        unsigned screenofs = dest;

        for (row = 0; row < height; row++)
        {
            for (col = 0; col < width; col++)
            {
                screenbuffer[plane][VRAM_WRAP(screenofs + col)] = *src;
                src++;
            }
            screenofs += linewidth;
        }
    }
}


//===========================================================================

/*
====================
=
= VW_ScreenToMem
=
====================
*/

void VW_ScreenToMem(unsigned source, memptr dest, unsigned width, unsigned height)
{
    uint8_t *dst = (uint8_t *)dest;
    int plane;
    unsigned row, col;

    for (plane = 0; plane < 4; plane++)
    {
        unsigned screenofs = source;

        for (row = 0; row < height; row++)
        {
            for (col = 0; col < width; col++)
            {
                *dst = screenbuffer[plane][VRAM_WRAP(screenofs + col)];
                dst++;
            }
            screenofs += linewidth;
        }
    }
}


//===========================================================================

#if NUMPICS>0

void VW_DrawPic(unsigned x, unsigned y, unsigned chunknum)
{
    int picnum = chunknum - STARTPICS;
    memptr source;
    unsigned dest,width,height;

    source = grsegs[chunknum];
    dest = ylookup[y]+x+bufferofs;
    width = pictable[picnum].width;
    height = pictable[picnum].height;

    VW_MemToScreen(source,dest,width,height);
}

#endif

#if NUMPICM>0

void VW_DrawMPic(unsigned x, unsigned y, unsigned chunknum)
{
    int picnum = chunknum - STARTPICM;
    memptr source;
    unsigned dest,width,height;

    source = grsegs[chunknum];
    dest = ylookup[y]+x+bufferofs;
    width = picmtable[picnum].width;
    height = picmtable[picnum].height;

    VW_MaskBlock(source,0,dest,width,height,width*height);
}

void VW_ClipDrawMPic(unsigned x, int y, unsigned chunknum)
{
    int picnum = chunknum - STARTPICM;
    memptr source;
    unsigned dest,width,ofs,plane;
    int     height;

    source = grsegs[chunknum];
    width = picmtable[picnum].width;
    height = picmtable[picnum].height;
    plane = width*height;

    ofs = 0;
    if (y<0)
    {
        ofs= -y*width;
        height+=y;
        y=0;
    }
    else if (y+height>216)
    {
        height-=(y-216);
    }
    dest = ylookup[y]+x+bufferofs;
    if (height<1)
        return;

    VW_MaskBlock(source,ofs,dest,width,height,plane);
}

#endif

//===========================================================================

#if NUMSPRITES>0

void VW_DrawSprite(int x, int y, unsigned chunknum)
{
    spritetabletype far *spr;
    spritetype _seg *block;
    unsigned    dest,shift;

    spr = &spritetable[chunknum-STARTSPRITES];
    block = (spritetype _seg *)grsegs[chunknum];

    y+=spr->orgy>>G_P_SHIFT;
    x+=spr->orgx>>G_P_SHIFT;

    shift = (x&7)/2;

    dest = bufferofs + ylookup[y];
    if (x>=0)
        dest += x/SCREENXDIV;
    else
        dest += (x+1)/SCREENXDIV;

    VW_MaskBlock (block,block->sourceoffset[shift],dest,
        block->width[shift],spr->height,block->planesize[shift]);
}

#endif

//===========================================================================

/*
=============================================================================

                        FONT DRAWING ROUTINES

=============================================================================
*/

#if NUMFONT+NUMFONTM>0

static void
VWL_MeasureString (char far *string, word *width, word *height, fontstruct _seg *font)
{
    *height = font->height;
    for (*width = 0;*string;string++)
        *width += font->width[*((byte far *)string)];
}

void    VW_MeasurePropString (char far *string, word *width, word *height)
{
    VWL_MeasureString(string,width,height,(fontstruct _seg *)grsegs[STARTFONT+fontnumber]);
}

void    VW_MeasureMPropString  (char far *string, word *width, word *height)
{
    VWL_MeasureString(string,width,height,(fontstruct _seg *)grsegs[STARTFONTM+fontnumber]);
}

#endif


//===========================================================================

/*
=============================================================================

                    FONT RENDERING

=============================================================================
*/

#if NUMFONT+NUMFONTM>0

static void ShiftPropChar(unsigned ch, fontstruct *font)
{
    unsigned charw, charh;
    unsigned *shifttable;
    uint8_t *chardata;
    unsigned srcbytes;
    unsigned row, col;

    charw = (unsigned)(unsigned char)font->width[ch];
    charh = font->height;
    chardata = (uint8_t *)font + font->location[ch];

    srcbytes = (charw + 7) / 8;

    shifttable = shifttabletable[bufferbit];

    for (row = 0; row < charh; row++)
    {
        unsigned dibuf = bufferbyte;
        for (col = 0; col < srcbytes; col++)
        {
            uint8_t srcval = chardata[col];
            unsigned shifted = shifttable[srcval];
            uint8_t lo = shifted & 0xFF;
            uint8_t hi = (shifted >> 8) & 0xFF;

            databuffer[row * BUFFWIDTH + dibuf] |= lo;
            if (dibuf + 1 < BUFFWIDTH)
                databuffer[row * BUFFWIDTH + dibuf + 1] = hi;
            dibuf++;
        }
        chardata += srcbytes;
    }

    {
        unsigned newbit = bufferbit + charw;
        bufferbit = newbit & 7;
        bufferbyte += newbit >> 3;
    }
}


static void BufferToScreen(uint8_t *buf, unsigned bwidth, unsigned bheight,
    byte drawmode, byte color)
{
    unsigned row, col;
    unsigned scrofs;
    int plane;

    if (bwidth == 0)
        return;

    scrofs = screenspot;

    for (row = 0; row < bheight; row++)
    {
        for (col = 0; col < bwidth; col++)
        {
            uint8_t val = buf[row * BUFFWIDTH + col];
            unsigned pos = VRAM_WRAP(scrofs + col);

            if (drawmode == 8)
            {
                // AND mode
                for (plane = 0; plane < 4; plane++)
                    screenbuffer[plane][pos] &= val;
            }
            else if (drawmode == 16)
            {
                // OR mode
                for (plane = 0; plane < 4; plane++)
                {
                    if (color & (1 << plane))
                        screenbuffer[plane][pos] |= val;
                }
            }
            else
            {
                // XOR mode (default, 0x18/24)
                for (plane = 0; plane < 4; plane++)
                {
                    if (color & (1 << plane))
                        screenbuffer[plane][pos] ^= val;
                }
            }
        }
        scrofs += linewidth;
    }
}


#if NUMFONT

void VW_DrawPropString (char far *string)
{
    fontstruct *font;
    unsigned i;

    font = (fontstruct *)grsegs[STARTFONT+fontnumber];

    for (i = 0; i < BUFFHEIGHT; i++)
        databuffer[i * BUFFWIDTH] = 0;

    bufferbit = px & 7;
    bufferbyte = 0;

    while (*string)
    {
        ShiftPropChar((unsigned)(unsigned char)*string, font);
        string++;
    }

    screenspot = ylookup[py] + bufferofs + panadjust + (px >> 3);

    bufferwidth = bufferbyte;
    if (bufferbit)
        bufferwidth++;
    bufferheight = font->height;

    px += (bufferbyte << 3) | bufferbit;

    BufferToScreen(databuffer, bufferwidth, bufferheight, pdrawmode, fontcolor);
}

#endif


#if NUMFONTM

static void ShiftMPropChar(unsigned ch, fontstruct *font)
{
    unsigned charw, charh;
    unsigned *shifttable;
    uint8_t *chardata;
    unsigned srcbytes;
    unsigned row, col;

    charw = (unsigned)(unsigned char)font->width[ch];
    charh = font->height;
    chardata = (uint8_t *)font + font->location[ch];

    srcbytes = (charw + 7) / 8;

    shifttable = shifttabletable[bufferbit];

    // First half: mask data (NOT shift)
    for (row = 0; row < charh; row++)
    {
        unsigned dibuf = bufferbyte;
        for (col = 0; col < srcbytes; col++)
        {
            uint8_t srcval = chardata[col];
            unsigned shifted = shifttable[srcval];
            uint8_t lo = shifted & 0xFF;
            uint8_t hi = (shifted >> 8) & 0xFF;

            databuffer[row * BUFFWIDTH + dibuf] &= ~lo;
            if (dibuf + 1 < BUFFWIDTH)
                databuffer[row * BUFFWIDTH + dibuf + 1] &= ~hi;
            dibuf++;
        }
        chardata += srcbytes;
    }

    // Second half: color data (OR shift)
    for (row = 0; row < charh; row++)
    {
        unsigned dibuf = bufferbyte;
        unsigned datarow = (charh + row) * BUFFWIDTH;
        for (col = 0; col < srcbytes; col++)
        {
            uint8_t srcval = chardata[col];
            unsigned shifted = shifttable[srcval];
            uint8_t lo = shifted & 0xFF;
            uint8_t hi = (shifted >> 8) & 0xFF;

            databuffer[datarow + dibuf] |= lo;
            if (dibuf + 1 < BUFFWIDTH)
                databuffer[datarow + dibuf + 1] = hi;
            dibuf++;
        }
        chardata += srcbytes;
    }

    {
        unsigned newbit = bufferbit + charw;
        bufferbit = newbit & 7;
        bufferbyte += newbit >> 3;
    }
}


void VW_DrawMPropString (char far *string)
{
    fontstruct *font;
    unsigned i, charh;

    font = (fontstruct *)grsegs[STARTFONTM+fontnumber];
    charh = font->height;

    for (i = 0; i < charh; i++)
    {
        databuffer[i * BUFFWIDTH] = 0xFF;
        databuffer[(charh + i) * BUFFWIDTH] = 0x00;
    }

    bufferbit = px & 7;
    bufferbyte = 0;

    while (*string)
    {
        ShiftMPropChar((unsigned)(unsigned char)*string, font);
        string++;
    }

    screenspot = ylookup[py] + bufferofs + panadjust + (px >> 3);

    bufferwidth = bufferbyte;
    if (bufferbit)
        bufferwidth++;
    bufferheight = charh;

    px += (bufferbyte << 3) | bufferbit;

    // First pass: AND mask
    BufferToScreen(databuffer, bufferwidth, bufferheight, 8, 15);

    // Second pass: OR color data
    BufferToScreen(&databuffer[charh * BUFFWIDTH], bufferwidth, bufferheight,
        16, fontcolor);
}

#endif  // NUMFONTM

#endif  // NUMFONT+NUMFONTM


//===========================================================================

/*
=============================================================================

                        CURSOR ROUTINES

=============================================================================
*/

static void VWL_DrawCursor (void)
{
    cursorspot = bufferofs + ylookup[cursory+pansy]+(cursorx+pansx)/SCREENXDIV;
    VW_ScreenToMem(cursorspot,cursorsave,cursorwidth,cursorheight);
    VWB_DrawSprite(cursorx,cursory,cursornumber);
}

static void VWL_EraseCursor (void)
{
    VW_MemToScreen(cursorsave,cursorspot,cursorwidth,cursorheight);
    VW_MarkUpdateBlock ((cursorx+pansx)&SCREENXMASK,cursory+pansy,
        ( (cursorx+pansx)&SCREENXMASK)+cursorwidth*SCREENXDIV-1,
        cursory+pansy+cursorheight-1);
}

void VW_ShowCursor (void)
{
    cursorvisible++;
}

void VW_HideCursor (void)
{
    cursorvisible--;
}

void VW_MoveCursor (int x, int y)
{
    cursorx = x;
    cursory = y;
}

void VW_SetCursor (int spritenum)
{
    VW_FreeCursor ();

    cursornumber = spritenum;

    CA_CacheGrChunk (spritenum);
    MM_SetLock (&grsegs[spritenum],true);

    cursorwidth = spritetable[spritenum-STARTSPRITES].width+1;
    cursorheight = spritetable[spritenum-STARTSPRITES].height;

    MM_GetPtr (&cursorsave,cursorwidth*cursorheight*5);
    MM_SetLock (&cursorsave,true);
}

void VW_FreeCursor (void)
{
    if (cursornumber)
    {
        MM_SetLock (&grsegs[cursornumber],false);
        MM_SetPurge (&grsegs[cursornumber],3);
        MM_SetLock (&cursorsave,false);
        MM_FreePtr (&cursorsave);
        cursornumber = 0;
    }
}


/*
=============================================================================

        Double buffer management routines

=============================================================================
*/

void VW_InitDoubleBuffer (void)
{
    VW_SetScreen (displayofs+panadjust,0);
}

void VW_FixRefreshBuffer (void)
{
    VW_ScreenToScreen (displayofs,bufferofs,PORTTILESWIDE*4*CHARWIDTH,
        (PORTTILESHIGH-1)*16);
}

void VW_QuitDoubleBuffer (void)
{
}

int VW_MarkUpdateBlock (int x1, int y1, int x2, int y2)
{
    int x,y,xt1,yt1,xt2,yt2,nextline;
    byte *mark;

    xt1 = x1>>PIXTOBLOCK;
    yt1 = y1>>PIXTOBLOCK;

    xt2 = x2>>PIXTOBLOCK;
    yt2 = y2>>PIXTOBLOCK;

    if (xt1<0)
        xt1=0;
    else if (xt1>=UPDATEWIDE-1)
        return 0;

    if (yt1<0)
        yt1=0;
    else if (yt1>UPDATEHIGH)
        return 0;

    if (xt2<0)
        return 0;
    else if (xt2>=UPDATEWIDE-1)
        xt2 = UPDATEWIDE-2;

    if (yt2<0)
        return 0;
    else if (yt2>=UPDATEHIGH)
        yt2 = UPDATEHIGH-1;

    mark = updateptr + uwidthtable[yt1] + xt1;
    nextline = UPDATEWIDE - (xt2-xt1) - 1;

    for (y=yt1;y<=yt2;y++)
    {
        for (x=xt1;x<=xt2;x++)
            *mark++ = 1;

        mark += nextline;
    }

    return 1;
}


static void VWL_UpdateScreenBlocks (void)
{
    byte *scanptr;
    byte *endptr;
    int plane;

    scanptr = updateptr;
    endptr = updateptr + UPDATEWIDE*UPDATEHIGH;

    while (scanptr < endptr)
    {
        if (*scanptr)
        {
            byte *runstart = scanptr;
            unsigned tilecount;
            unsigned screenstart, srcstart;
            unsigned row, col, bytewidth;

            while (scanptr < endptr && *scanptr)
                scanptr++;

            tilecount = scanptr - runstart;

            screenstart = blockstarts[runstart - updateptr] + displayofs;
            srcstart = blockstarts[runstart - updateptr] + bufferofs;

            bytewidth = tilecount * 2;
            for (plane = 0; plane < 4; plane++)
            {
                for (row = 0; row < 16; row++)
                {
                    unsigned srow = srcstart + row * linewidth;
                    unsigned drow = screenstart + row * linewidth;
                    for (col = 0; col < bytewidth; col++)
                    {
                        screenbuffer[plane][VRAM_WRAP(drow + col)] = screenbuffer[plane][VRAM_WRAP(srow + col)];
                    }
                }
            }

            memset(runstart, 0, tilecount);
        }
        else
        {
            scanptr++;
        }
    }

    *(unsigned *)(updateptr + UPDATEWIDE*PORTTILESHIGH) = UPDATETERMINATE;
}


void VW_UpdateScreen (void)
{
    // Poll SDL events every frame. On original DOS, the keyboard ISR and
    // timer interrupt updated LastScan/TimeCount automatically. With SDL
    // we must pump events explicitly so that input works in all game loops
    // (e.g. the control panel menu) that check LastScan without calling
    // dedicated input-wait functions.
    IN_PumpEvents();

    if (cursorvisible>0)
        VWL_DrawCursor();

    VWL_UpdateScreenBlocks();

    if (cursorvisible>0)
        VWL_EraseCursor();

    VW_Present();
}

void VW_CGAFullUpdate (void)
{
    displayofs = bufferofs+panadjust;
    memset(updateptr, 0, UPDATEWIDE*UPDATEHIGH);
    *(unsigned *)(updateptr + UPDATEWIDE*PORTTILESHIGH) = UPDATETERMINATE;
}


//===========================================================================

/*
=============================================================================

           VWB_ DOUBLE-BUFFERED DRAWING ROUTINES

=============================================================================
*/


void VWB_DrawTile8 (int x, int y, int tile)
{
    x+=pansx;
    y+=pansy;
    if (VW_MarkUpdateBlock (x&SCREENXMASK,y,(x&SCREENXMASK)+7,y+7))
        VW_DrawTile8 (x/SCREENXDIV,y,tile);
}

void VWB_DrawTile8M (int x, int y, int tile)
{
    int xb;

    x+=pansx;
    y+=pansy;
    xb = x/SCREENXDIV;
    if (VW_MarkUpdateBlock (x&SCREENXMASK,y,(x&SCREENXMASK)+7,y+7))
        VW_DrawTile8M (xb,y,tile);
}

void VWB_DrawTile16 (int x, int y, int tile)
{
    if (!tile)
        Quit("Null tile!");

    x+=pansx;
    y+=pansy;
    if (VW_MarkUpdateBlock (x&SCREENXMASK,y,(x&SCREENXMASK)+15,y+15))
        VW_DrawTile16 (x/SCREENXDIV,y,tile);
}

void VWB_DrawTile16M (int x, int y, int tile)
{
    int xb;

    x+=pansx;
    y+=pansy;
    xb = x/SCREENXDIV;
    if (VW_MarkUpdateBlock (x&SCREENXMASK,y,(x&SCREENXMASK)+15,y+15))
        VW_DrawTile16M (xb,y,tile);
}

#if NUMPICS
void VWB_DrawPic (int x, int y, int chunknum)
{
    int picnum = chunknum - STARTPICS;
    memptr source;
    unsigned dest,width,height;

    x+=pansx;
    y+=pansy;
    x/= SCREENXDIV;

    source = grsegs[chunknum];
    dest = ylookup[y]+x+bufferofs;
    width = pictable[picnum].width;
    height = pictable[picnum].height;

    if (VW_MarkUpdateBlock (x*SCREENXDIV,y,(x+width)*SCREENXDIV-1,y+height-1))
        VW_MemToScreen(source,dest,width,height);
}
#endif

#if NUMPICM>0
void VWB_DrawMPic(int x, int y, int chunknum)
{
    int picnum = chunknum - STARTPICM;
    memptr source;
    unsigned dest,width,height;

    x+=pansx;
    y+=pansy;
    x/=SCREENXDIV;

    source = grsegs[chunknum];
    dest = ylookup[y]+x+bufferofs;
    width = picmtable[picnum].width;
    height = picmtable[picnum].height;

    if (VW_MarkUpdateBlock (x*SCREENXDIV,y,(x+width)*SCREENXDIV-1,y+height-1))
        VW_MaskBlock(source,0,dest,width,height,width*height);
}
#endif


void VWB_Bar (int x, int y, int width, int height, int color)
{
    x+=pansx;
    y+=pansy;
    if (VW_MarkUpdateBlock (x,y,x+width,y+height-1) )
        VW_Bar (x,y,width,height,color);
}


#if NUMFONT
void VWB_DrawPropString  (char far *string)
{
    int x,y;
    x = px+pansx;
    y = py+pansy;
    VW_DrawPropString (string);
    VW_MarkUpdateBlock(x,y,x+bufferwidth*8-1,y+bufferheight-1);
}
#endif


#if NUMFONTM
void VWB_DrawMPropString (char far *string)
{
    int x,y;
    x = px+pansx;
    y = py+pansy;
    VW_DrawMPropString (string);
    VW_MarkUpdateBlock(x,y,x+bufferwidth*8-1,y+bufferheight-1);
}
#endif

#if NUMSPRITES
void VWB_DrawSprite(int x, int y, int chunknum)
{
    spritetabletype far *spr;
    spritetype _seg *block;
    unsigned    dest,shift,width,height;

    x+=pansx;
    y+=pansy;

    spr = &spritetable[chunknum-STARTSPRITES];
    block = (spritetype _seg *)grsegs[chunknum];

    y+=spr->orgy>>G_P_SHIFT;
    x+=spr->orgx>>G_P_SHIFT;

    shift = (x&7)/2;

    dest = bufferofs + ylookup[y];
    if (x>=0)
        dest += x/SCREENXDIV;
    else
        dest += (x+1)/SCREENXDIV;

    width = block->width[shift];
    height = spr->height;

    if (VW_MarkUpdateBlock (x&SCREENXMASK,y,(x&SCREENXMASK)+width*SCREENXDIV-1
        ,y+height-1))
        VW_MaskBlock (block,block->sourceoffset[shift],dest,
            width,height,block->planesize[shift]);
}
#endif

void VWB_Plot (int x, int y, int color)
{
    x+=pansx;
    y+=pansy;
    if (VW_MarkUpdateBlock (x,y,x,y))
        VW_Plot(x,y,color);
}

void VWB_Hlin (int x1, int x2, int y, int color)
{
    x1+=pansx;
    x2+=pansx;
    y+=pansy;
    if (VW_MarkUpdateBlock (x1,y,x2,y))
        VW_Hlin(x1,x2,y,color);
}

void VWB_Vlin (int y1, int y2, int x, int color)
{
    x+=pansx;
    y1+=pansy;
    y2+=pansy;
    if (VW_MarkUpdateBlock (x,y1,x,y2))
        VW_Vlin(y1,y2,x,color);
}
