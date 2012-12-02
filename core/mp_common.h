/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
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

#ifndef MPLAYER_MPCOMMON_H
#define MPLAYER_MPCOMMON_H

#include <stdlib.h>
#include <stdbool.h>

#include "compat/compiler.h"
#include "core/mp_talloc.h"

// both int64_t and double should be able to represent this exactly
#define MP_NOPTS_VALUE (-1LL<<63)

#define ROUND(x) ((int)((x) < 0 ? (x) - 0.5 : (x) + 0.5))

extern const char *mplayer_version;
extern const char *mplayer_builddate;

char *mp_format_time(double time, bool fractions);

struct mp_rect {
    int x0, y0;
    int x1, y1;
};

#endif /* MPLAYER_MPCOMMON_H */
