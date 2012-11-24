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

#include "audio/format.h"
#include "audio/mp_audio.h"
#include "core/mp_msg.h"

#include <libavutil/mem.h>

static int mp_audio_n_planes(struct mp_audio *audio)
{
    if ((audio->format & AF_FORMAT_INTERLEAVING_MASK) == AF_FORMAT_PLANAR)
        return audio->nch;
    else
        return 1;
}

void mp_audio_alloc_planes(struct mp_audio *audio, int len)
{
    if (!audio || audio->allocated) return;

    int plane_len = ((double)len / mp_audio_n_planes(audio)) + 0.5;

    for (int i = 0; i < mp_audio_n_planes(audio); i++) {
        audio->planes[i] = av_mallocz(plane_len);

        if (!audio->planes[i]) {
            mp_msg(MSGT_AFILTER, MSGL_FATAL, "Failed to allocate audio planes\n");
            abort();
        }
    }

    audio->len = len;
    audio->allocated = 1;
}

void mp_audio_free_planes(struct mp_audio *audio)
{
    if (!audio || !audio->allocated) return;
    for (int i = 0; i < mp_audio_n_planes(audio); i++)
        if (audio->planes[i])
            av_freep(&audio->planes[i]);

    audio->allocated = 0;
}

struct mp_audio *mp_audio_new(void *ctx, int len)
{
    struct mp_audio *rv = talloc_zero(ctx, struct mp_audio);
    mp_audio_alloc_planes(rv, len);
    return rv;
};

void mp_audio_free(struct mp_audio *audio)
{
    mp_audio_free_planes(audio);
    talloc_free(audio);
}

