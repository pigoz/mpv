/*
 * This file is part of mpv.
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
 * with mpv; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPV_MACOSX_APPLICATION
#define MPV_MACOSX_APPLICATION

#include "core/mp_core.h"

// Runs the Cocoa Main Event Loop
void cocoa_run_runloop(void);
void cocoa_post_fake_event(void);

// Definition of the playloop callback function pointer
typedef void(*play_loop_callback)(struct MPContext *);

// Adds play_loop as a timer of the Main Cocoa Event Loop
void cocoa_run_loop_schedule(play_loop_callback callback,
                             struct MPContext *context);

#endif /* MPV_MACOSX_APPLICATION */
