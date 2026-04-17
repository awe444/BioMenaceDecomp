/* ID_RF_A.C - C port of ID_RF_A.ASM
 *
 * EGA refresh routines for the tile rendering system.
 * Translated from the original x86 assembly to C for the SDL2 port.
 *
 * These functions handle:
 * - RFL_NewTile: Drawing composite tiles to the master screen
 * - RFL_UpdateTiles: Copying changed tiles from master to buffer screen
 * - RFL_MaskForegroundTiles: Drawing masked foreground tiles on the buffer
 */

#include "ID_HEADS.H"

/*
 * External variables from ID_RF.c that the original ASM accessed via EXTRN.
 * Most are already declared in ID_RF.H, but a few are only in ID_RF.c.
 */
extern unsigned originmap;
extern byte *updatestart[2];

#define TILEWIDTH_EGA  2

/* Size of the update scan area: (PORTTILESWIDE+1) * PORTTILESHIGH */
#define UPDATE_SCAN_SIZE  ((PORTTILESWIDE + 1) * PORTTILESHIGH)


/*
=================
=
= RFL_NewTile
=
= Draws a composite two plane tile to the master screen and sets the update
= spot to 1 in both update pages, forcing the tile to be copied to the
= view pages the next two refreshes.
=
= Called to draw newly scrolled on strips and animating tiles.
=
=================
*/

void RFL_NewTile(unsigned updateoffset)
{
	unsigned mapofs, screenofs, bg, fg;

	//
	// mark both update lists at this spot
	//
	updatestart[0][updateoffset] = 1;
	updatestart[1][updateoffset] = 1;

	//
	// calculate the location in screenseg to draw the tile
	//
	mapofs = updatemapofs[updateoffset] + originmap;
	screenofs = blockstarts[updateoffset] + masterofs;

	//
	// get the foreground and background tile numbers from the map planes
	// mapofs is a byte offset; divide by 2 to get the uint16_t word index
	//
	fg = mapsegs[1][mapofs / 2];   // foreground map plane
	bg = mapsegs[0][mapofs / 2];   // background map plane

	if (fg == 0)
	{
		//
		// No foreground tile, so draw a single background tile
		//
		if (grsegs[STARTTILE16 + bg])
			VW_MemToScreen(grsegs[STARTTILE16 + bg], screenofs,
				TILEWIDTH_EGA, 16);
	}
	else
	{
		//
		// Draw a masked tile combo: background first, then masked foreground
		//
		if (grsegs[STARTTILE16 + bg])
			VW_MemToScreen(grsegs[STARTTILE16 + bg], screenofs,
				TILEWIDTH_EGA, 16);
		if (grsegs[STARTTILE16M + fg])
			VW_MaskBlock(grsegs[STARTTILE16M + fg], 0, screenofs,
				TILEWIDTH_EGA, 16, TILEWIDTH_EGA * 16);
	}
}


/*
=================
=
= RFL_UpdateTiles
=
= Scans through the update matrix pointed to by updateptr, looking for 1s.
= A 1 represents a tile that needs to be copied from the master screen to the
= current screen (a new row or an animated tile). If more than one adjacent
= tile in a horizontal row needs to be copied, they will be copied as a group.
=
= In the original EGA code, this runs in write mode 1 which copies all 4
= planes simultaneously. VW_ScreenToScreen handles this in the SDL port.
=
=================
*/

void RFL_UpdateTiles(void)
{
	byte *scan, *end, *blockstart_ptr;
	unsigned updateoffset, count;
	unsigned src, dest;

	scan = updateptr;
	end = updateptr + UPDATE_SCAN_SIZE;

	while (scan < end)
	{
		//
		// scan for a 1 in the update list
		//
		while (scan < end && *scan != 1)
			scan++;

		if (scan >= end)
			break;

		//
		// found a tile that needs updating - count consecutive 1s
		//
		updateoffset = (unsigned)(scan - updateptr);
		blockstart_ptr = scan;

		while (scan < end && *scan == 1)
			scan++;

		count = (unsigned)(scan - blockstart_ptr);

		//
		// copy tile(s) from master screen to current buffer
		//
		src = blockstarts[updateoffset] + masterofs;
		dest = blockstarts[updateoffset] + bufferofs;

		VW_ScreenToScreen(src, dest, count * TILEWIDTH_EGA, 16);
	}
}


/*
=================
=
= RFL_MaskForegroundTiles
=
= Scan through update looking for 3's. If the foreground tile there is a
= masked foreground tile, draw it to the screen.
=
= 3's are set by RFL_UpdateSprites when a sprite overlaps a tile position.
= This ensures masked foreground tiles are drawn on top of sprites.
=
=================
*/

void RFL_MaskForegroundTiles(void)
{
	byte *scan, *end;
	unsigned updateoffset;
	unsigned mapofs, fg;
	unsigned dest;

	scan = updateptr;
	end = updateptr + UPDATE_SCAN_SIZE;

	while (scan < end)
	{
		//
		// scan for a 3 in the update list
		//
		if (*scan != 3)
		{
			scan++;
			continue;
		}

		updateoffset = (unsigned)(scan - updateptr);
		scan++;

		//
		// found a tile marked 3, see if it needs a masked foreground
		// mapofs is a byte offset; divide by 2 to get the uint16_t word index
		//
		mapofs = updatemapofs[updateoffset] + originmap;
		fg = mapsegs[1][mapofs / 2];   // foreground tile number

		if (fg == 0)
			continue;   // no foreground tile

		if (!(tinf[INTILE + fg] & 0x80))
			continue;   // not a masked tile (high bit = masked)

		//
		// draw the masked foreground tile to the buffer screen
		//
		dest = blockstarts[updateoffset] + bufferofs;

		if (grsegs[STARTTILE16M + fg])
			VW_MaskBlock(grsegs[STARTTILE16M + fg], 0, dest,
				TILEWIDTH_EGA, 16, TILEWIDTH_EGA * 16);
	}
}

