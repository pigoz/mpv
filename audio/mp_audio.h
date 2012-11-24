/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPV_MP_AUDIO_H
#define MPV_MP_AUDIO_H

#define MP_NCH 8

// Audio data chunk
struct mp_audio {
    void *planes[MP_NCH]; // data buffer
    int free_offset;      // offset to free chunk of each plane
    int allocated;        // whether the plane buffers are allocated
    int len;              // total buffer length (sum of length of all planes)
    int rate;             // sample rate
    int nch;              // number of channels
    int format;           // format
    int bps;              // bytes per sample
};

// allocates and initializes a new mp_audio instance
struct mp_audio *mp_audio_new(void *ctx, int len);

// frees up a mp_audio instance
void mp_audio_free(struct mp_audio *);

// allocates planes for a given mp_audio instance
// (this function is idempotent)
void mp_audio_alloc_planes(struct mp_audio *, int len);

// deallocates planes for a given mp_audio instance
// (this function is idempotent)
void mp_audio_free_planes(struct mp_audio *);

#endif /* MPV_MP_AUDIO_H */
