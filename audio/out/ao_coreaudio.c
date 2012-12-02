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

#include "coreaudio/common.h"
#include "coreaudio/format.h"

struct ca_priv {
    AudioStreamBasicDescription asbd;
};

static void ca_uninit(struct ao *ao, bool cut_audio) {};
static int ca_init(struct ao *ao, char *params) {
    struct ca_priv *priv = talloc_zero(ao, struct ca_priv);
    ao->priv = priv;
    ca_fillout_asbd_for_lpcm2(&priv->asbd, ao);
    ca_print_asbd(MSGL_WARN, "initializing with ASBD: ", &priv->asbd);

    return 0;
};
static int ca_play(struct ao *ao, void *data, int len, int flags) { return 0;};
static int ca_get_space(struct ao *ao) { return 0;};
static float ca_get_delay(struct ao *ao) { return 0.0; };
static void ca_reset(struct ao *ao) { };
static void ca_pause(struct ao *ao) { };
static void ca_resume(struct ao *ao) { };

const struct ao_driver audio_out_coreaudio = {
    .is_new = true,
    .info = &(const struct ao_info) {
        "CoreAudio",
        "coreaudio",
        "Stefano Pigozzi",
        "",
    },
    .uninit    = ca_uninit,
    .init      = ca_init,
    .play      = ca_play,
    .get_space = ca_get_space,
    .get_delay = ca_get_delay,
    .reset     = ca_reset,
    .pause     = ca_pause,
    .resume    = ca_resume,
};
