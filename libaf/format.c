/*
 * Copyright (C) 2005 Alex Beregszaszi
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>

#include "af.h"

struct AFSampleFormatInfo {
   enum AFFormat format;
   int  bits;
   int  planar;
   int  endian;
};

struct AFSampleFormatRelations {
   enum AFFormat format;
   enum AFFormat alternate;
   enum AFFormat swapped;
   enum AFFormat altplanar;
};

// define sample format attributes for common lpcm formats
#define DEF_FMT(X, b) \
    [AF_FORMAT_##X##b##_LE]  = { .bits = b, .planar = 0, .endian = 0, }, \
    [AF_FORMAT_##X##b##_BE]  = { .bits = b, .planar = 0, .endian = 1, }, \
    [AF_FORMAT_##X##b##P_LE] = { .bits = b, .planar = 1, .endian = 0, }, \
    [AF_FORMAT_##X##b##P_BE] = { .bits = b, .planar = 1, .endian = 1, },

static const struct AFSampleFormatInfo sample_fmt_info[AF_FORMAT_NB] = {
    [AF_FORMAT_U8]  = { .bits = 8, .planar = 0, .endian = -1, }, \
    [AF_FORMAT_S8]  = { .bits = 8, .planar = 0, .endian = -1, }, \
    [AF_FORMAT_U8P] = { .bits = 8, .planar = 1, .endian = -1, }, \
    [AF_FORMAT_S8P] = { .bits = 8, .planar = 1, .endian = -1, }, \

    DEF_FMT(U, 16)
    DEF_FMT(S, 16)

    DEF_FMT(U, 24)
    DEF_FMT(S, 24)

    DEF_FMT(U, 32)
    DEF_FMT(S, 32)

    [AF_FORMAT_AC3_LE] = { .bits = 16, .planar = 1, .endian = 0 },
    [AF_FORMAT_AC3_BE] = { .bits = 16, .planar = 1, .endian = 1 },
};

#undef DEF_FMT

int af_fmt2bits(int format)
{
    return sample_fmt_info[format].bits;
}

int af_bits2fmt(int bits)
{
    return (bits / 8 - 1) << 3;
}

/* Convert format to str input str is a buffer for the
   converted string, size is the size of the buffer */
char *af_fmt2str(int format, char *str, int size)
{
    const char *name = af_fmt2str_short(format);
    if (name)
        snprintf(str, size, "%s", name);
    else
        snprintf(str, size, "%#x", format);
    return str;
}

const char *af_fmt2str_short(int format)
{
    int i;

    for (i = 0; af_fmtstr_table[i].name; i++)
        if (af_fmtstr_table[i].format == format)
            return af_fmtstr_table[i].name;

    return "??";
}

static bool af_fmt_valid(int format)
{
    return format > AF_FORMAT_UNKNOWN && format < AF_FORMAT_NB;
}

int af_str2fmt_short(bstr str)
{
    if (bstr_startswith0(str, "0x")) {
        bstr rest;
        int fmt = bstrtoll(str, &rest, 16);
        if (rest.len == 0 && af_fmt_valid(fmt))
            return fmt;
    }

    for (int i = 0; af_fmtstr_table[i].name; i++)
        if (!bstrcasecmp0(str, af_fmtstr_table[i].name))
            return af_fmtstr_table[i].format;

    return -1;
}
