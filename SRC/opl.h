/* opl.h - OPL2 emulation interface for the BioMenace SDL port
 *
 * Wraps the Nuked OPL3 emulator (LGPL 2.1+) to provide a simple
 * OPL2-level interface for the sound manager.
 *
 * Copyright (C) 2024 BioMenaceDecomp contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef OPL_H
#define OPL_H

#include <stdint.h>

/* Initialize the OPL emulator at the given sample rate */
void OPL_Init(uint32_t sampleRate);

/* Shut down the OPL emulator */
void OPL_Shutdown(void);

/* Write a value to an OPL register (OPL2 register space: 0x00-0xF5) */
void OPL_WriteReg(uint8_t reg, uint8_t val);

/* Generate 'numSamples' mono 16-bit samples into 'buffer' */
void OPL_GenerateSamples(int16_t *buffer, uint32_t numSamples);

#endif /* OPL_H */
