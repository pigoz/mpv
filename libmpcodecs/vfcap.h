/* VFCAP_* values: they are flags, returned by query_format():
 *
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

#ifndef MPLAYER_VFCAP_H
#define MPLAYER_VFCAP_H

// set, if the given colorspace is supported (with or without conversion)
#define VFCAP_CSP_SUPPORTED 0x1
// set, if the given colorspace is supported _without_ conversion
#define VFCAP_CSP_SUPPORTED_BY_HW 0x2
// set if the driver/filter can draw OSD
#define VFCAP_OSD 0x4
// scaling up/down by hardware, or software:
#define VFCAP_HWSCALE_UP 0x10
#define VFCAP_HWSCALE_DOWN 0x20
#define VFCAP_SWSCALE 0x40
// driver/filter can do vertical flip (upside-down)
#define VFCAP_FLIP 0x80

// driver/hardware handles timing (blocking)
#define VFCAP_TIMER 0x100
// vf filter: accepts stride (put_image)
// vo driver: has draw_slice() support for the given csp
#define VFCAP_ACCEPT_STRIDE 0x400
// filter does postprocessing (so you shouldn't scale/filter image before it)
#define VFCAP_POSTPROC 0x800
// used by libvo and vf_vo, indicates the VO does not support draw_slice for this format
#define VOCAP_NOSLICES 0x8000

#endif /* MPLAYER_VFCAP_H */
