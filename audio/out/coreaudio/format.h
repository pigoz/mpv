/*
 * This file is part of mpv.
 * Copyright (C) 2012 Stefano Pigozzi
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

// Utilities for conversion between mpv's internal format and CoreAudio's

#ifndef MPV_CA_FORMAT_H
#define MPV_CA_FORMAT_H

#include "common.h"

void ca_print_asbd (
    int log_level,
    const char *message,
    const AudioStreamBasicDescription *asbd
);

// Calculates format flags for the ASBD
uint32_t ca_calc_asbd_lpcm_flags (
    bool is_float,
    bool is_signed_integer,
    bool is_big_endian,
    bool is_non_interleaved
);

// Fills a LPCM ASBD with the given data
void ca_fillout_asbd_for_lpcm (
   AudioStreamBasicDescription *asbd,
   Float64  samplerate,
   uint32_t channels,
   uint32_t bits_per_channel,
   bool     is_float,
   bool     is_signed_integer,
   bool     is_big_endian,
   bool     is_non_interleaved
);

// Fills a LPCM ASBD with data from the AO struct
void ca_fillout_asbd_for_lpcm2 (
    AudioStreamBasicDescription *asbd,
    struct ao *ao
);

#endif /* MPV_CA_FORMAT_H */
