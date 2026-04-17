/* opl.c - OPL2 emulation wrapper for the BioMenace SDL port
 *
 * Uses the Nuked OPL3 emulator (LGPL 2.1+) by Nuke.YKT running
 * in OPL2-compatible mode.
 *
 * Copyright (C) 2024 BioMenaceDecomp contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "opl.h"
#include "opl/opl3.h"
#include <string.h>

static opl3_chip opl_chip;
static uint32_t  opl_sampleRate;

void OPL_Init(uint32_t sampleRate)
{
    opl_sampleRate = sampleRate;
    OPL3_Reset(&opl_chip, sampleRate);
}

void OPL_Shutdown(void)
{
    memset(&opl_chip, 0, sizeof(opl_chip));
}

void OPL_WriteReg(uint8_t reg, uint8_t val)
{
    /* OPL2 registers live in bank 0 (0x000-0x0F5) */
    OPL3_WriteReg(&opl_chip, (uint16_t)reg, val);
}

void OPL_GenerateSamples(int16_t *buffer, uint32_t numSamples)
{
    /* Nuked OPL3 generates interleaved stereo (L, R) pairs.
     * We mix down to mono for the game's single-channel output.
     */
    int16_t stereo[2];
    uint32_t i;

    for (i = 0; i < numSamples; i++)
    {
        OPL3_GenerateResampled(&opl_chip, stereo);
        /* Average L+R to mono */
        buffer[i] = (int16_t)(((int32_t)stereo[0] + (int32_t)stereo[1]) / 2);
    }
}
