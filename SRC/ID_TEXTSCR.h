/* ID_TEXTSCR.h - SDL text mode screen renderer
 *
 * Emulates the DOS VGA text mode (80x25, segment 0xB800) using SDL2.
 * Provides a character+attribute buffer and renders it with a CP437 8x16 font.
 *
 * This module creates its own SDL window before VW_Startup() runs, and
 * destroys it when the game transitions to graphics mode.
 */

#ifndef ID_TEXTSCR_H
#define ID_TEXTSCR_H

#include <stdint.h>

/* Text mode dimensions */
#define TXT_COLS 80
#define TXT_ROWS 25
#define TXT_BUF_SIZE (TXT_COLS * TXT_ROWS * 2) /* char+attr pairs = 4000 bytes */

/* Initialize the text screen system: creates SDL window and renderer.
 * Must be called before any text buffer access. */
void TXT_Init(void);

/* Render the current text buffer contents to the SDL window. */
void TXT_Update(void);

/* Destroy the text screen SDL window/renderer.
 * Called when transitioning to graphics mode. */
void TXT_Shutdown(void);

/* Return a pointer to the text buffer (4000 bytes, char+attr pairs).
 * This replaces DOS VGA text memory at segment 0xB800. */
uint8_t *TXT_GetBuffer(void);

#endif /* ID_TEXTSCR_H */
