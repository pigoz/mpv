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

#ifndef MPV_CA_COMMON_H
#define MPV_CA_COMMON_H

#include <AudioUnit/AudioUnit.h>
#include "talloc.h"

#include "audio/format.h"
#include "audio/out/ao.h"

#include "core/mp_msg.h"

#define ca_msg(a, b...) mp_msg(MSGT_AO, a, "AO: [coreaudio] " b)

#endif /* MPV_CA_COMMON_H */
