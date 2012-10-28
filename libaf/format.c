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
   int  pl; // planar flag
   int  sn; // signed flag
   int  en; // little/big endian
};

struct AFSampleFormatRelation {
   enum AFFormat format;
   enum AFFormat r;
};

// define sample format attributes for common lpcm formats
#define DEF_FMT(b) \
    [AF_FORMAT_U##b##_LE]  = { .bits = b, .pl = 0, .en = 0, .sn = 0 }, \
    [AF_FORMAT_U##b##_BE]  = { .bits = b, .pl = 0, .en = 1, .sn = 0 }, \
    [AF_FORMAT_U##b##P_LE] = { .bits = b, .pl = 1, .en = 0, .sn = 0 }, \
    [AF_FORMAT_U##b##P_BE] = { .bits = b, .pl = 1, .en = 1, .sn = 0 }, \
    [AF_FORMAT_S##b##_LE]  = { .bits = b, .pl = 0, .en = 0, .sn = 1 }, \
    [AF_FORMAT_S##b##_BE]  = { .bits = b, .pl = 0, .en = 1, .sn = 1 }, \
    [AF_FORMAT_S##b##P_LE] = { .bits = b, .pl = 1, .en = 0, .sn = 1 }, \
    [AF_FORMAT_S##b##P_BE] = { .bits = b, .pl = 1, .en = 1, .sn = 1 }, \

static const struct AFSampleFormatInfo sample_fmt_info[AF_FORMAT_NB] = {
    [AF_FORMAT_U8]  = { .bits = 8, .pl = 0, .en = -1, .sn = 0 }, \
    [AF_FORMAT_S8]  = { .bits = 8, .pl = 0, .en = -1, .sn = 1 }, \
    [AF_FORMAT_U8P] = { .bits = 8, .pl = 1, .en = -1, .sn = 0 }, \
    [AF_FORMAT_S8P] = { .bits = 8, .pl = 1, .en = -1, .sn = 1 }, \

    DEF_FMT(16)
    DEF_FMT(24)
    DEF_FMT(32)

    [AF_FORMAT_FLT_LE] =  { .bits = -1, .pl = 0, .en = 0, .sn = -1 }, \
    [AF_FORMAT_FLT_BE] =  { .bits = -1, .pl = 0, .en = 1, .sn = -1 }, \
    [AF_FORMAT_FLTP_LE] = { .bits = -1, .pl = 1, .en = 0, .sn = -1 }, \
    [AF_FORMAT_FLTP_BE] = { .bits = -1, .pl = 1, .en = 1, .sn = -1 }, \

    [AF_FORMAT_AC3_LE] = { .bits = 16, .pl = 1, .en = 0, .sn = -1 },
    [AF_FORMAT_AC3_BE] = { .bits = 16, .pl = 1, .en = 1, .sn = -1 },
};

#undef DEF_FMT

#define DEF_FMT_ALT(b) \
    [AF_FORMAT_U##b##_LE]  = { .r = AF_FORMAT_S##b##_LE  }, \
    [AF_FORMAT_U##b##_BE]  = { .r = AF_FORMAT_S##b##_BE  }, \
    [AF_FORMAT_U##b##P_LE] = { .r = AF_FORMAT_S##b##P_LE }, \
    [AF_FORMAT_U##b##P_BE] = { .r = AF_FORMAT_S##b##P_BE }, \
    [AF_FORMAT_S##b##_LE]  = { .r = AF_FORMAT_U##b##_LE  }, \
    [AF_FORMAT_S##b##_BE]  = { .r = AF_FORMAT_U##b##_BE  }, \
    [AF_FORMAT_S##b##P_LE] = { .r = AF_FORMAT_U##b##P_LE }, \
    [AF_FORMAT_S##b##P_BE] = { .r = AF_FORMAT_U##b##P_BE }, \

static const struct AFSampleFormatRelation sample_fmt_alts[AF_FORMAT_NB] = {
    [AF_FORMAT_U8]  = { .r = AF_FORMAT_S8  },
    [AF_FORMAT_S8]  = { .r = AF_FORMAT_U8  },
    [AF_FORMAT_U8P] = { .r = AF_FORMAT_S8P },
    [AF_FORMAT_S8P] = { .r = AF_FORMAT_U8P },

    DEF_FMT_ALT(16)
    DEF_FMT_ALT(24)
    DEF_FMT_ALT(32)

    [AF_FORMAT_FLT_LE] =  { .r = AF_FORMAT_FLT_BE }, \
    [AF_FORMAT_FLT_BE] =  { .r = AF_FORMAT_FLT_LE }, \
    [AF_FORMAT_FLTP_LE] = { .r = AF_FORMAT_FLTP_BE }, \
    [AF_FORMAT_FLTP_BE] = { .r = AF_FORMAT_FLTP_LE }, \

    [AF_FORMAT_AC3_LE] = { .r = AF_FORMAT_AC3_LE },
    [AF_FORMAT_AC3_BE] = { .r = AF_FORMAT_AC3_BE },
};

#undef DEF_FMT_ALT

#define DEF_FMT_SWAP(b) \
    [AF_FORMAT_U##b##_LE]  = { .r = AF_FORMAT_U##b##_BE  }, \
    [AF_FORMAT_U##b##_BE]  = { .r = AF_FORMAT_U##b##_LE  }, \
    [AF_FORMAT_U##b##P_LE] = { .r = AF_FORMAT_U##b##P_BE }, \
    [AF_FORMAT_U##b##P_BE] = { .r = AF_FORMAT_U##b##P_LE }, \
    [AF_FORMAT_S##b##_LE]  = { .r = AF_FORMAT_S##b##_BE  }, \
    [AF_FORMAT_S##b##_BE]  = { .r = AF_FORMAT_S##b##_LE  }, \
    [AF_FORMAT_S##b##P_LE] = { .r = AF_FORMAT_S##b##P_BE }, \
    [AF_FORMAT_S##b##P_BE] = { .r = AF_FORMAT_S##b##P_LE }, \

static const struct AFSampleFormatRelation sample_fmt_swaps[AF_FORMAT_NB] = {
    [AF_FORMAT_U8]  = { .r = AF_FORMAT_U8 },
    [AF_FORMAT_S8]  = { .r = AF_FORMAT_S8 },
    [AF_FORMAT_U8P] = { .r = AF_FORMAT_U8P },
    [AF_FORMAT_S8P] = { .r = AF_FORMAT_S8P },

    DEF_FMT_SWAP(16)
    DEF_FMT_SWAP(24)
    DEF_FMT_SWAP(32)

    [AF_FORMAT_AC3_LE] = { .r = AF_FORMAT_AC3_BE },
    [AF_FORMAT_AC3_BE] = { .r = AF_FORMAT_AC3_LE },
};

#undef DEF_FMT_SWAP

int af_format_is_unsigned(enum AFFormat format)
{
    return sample_fmt_info[format].sn == 0;
}

int af_format_is_signed(enum AFFormat format)
{
    return sample_fmt_info[format].sn == 1;
}

enum AFFormat af_format_alternate(enum AFFormat format)
{
    return sample_fmt_alts[format].r;
}

int af_format_is_float(enum AFFormat format)
{
    return format >= AF_FORMAT_FLT_LE;
}

int af_format_is_integer(enum AFFormat format)
{
    return ! af_format_is_float(format);
}

int af_format_is_le(enum AFFormat format)
{
    return sample_fmt_info[format].en == 0;
}

int af_format_is_be(enum AFFormat format)
{
    return sample_fmt_info[format].en == 1;
}

int af_format_is_ne(enum AFFormat format)
{
#if BYTE_ORDER == BIG_ENDIAN
    int ne = 1;
#else
    int ne = 0;
#endif
    int en = sample_fmt_info[format].en;

    return en == -1 || en == ne;

}

enum AFFormat af_format_swapped(enum AFFormat format)
{
    return sample_fmt_swaps[format].r;
}

int af_format_is_planar(enum AFFormat format)
{
    return sample_fmt_info[format].pl;
}

int af_format_is_special(enum AFFormat format)
{
    return format >= AF_FORMAT_AC3_LE;
}

int af_format_is_ac3(enum AFFormat format)
{
    return format == AF_FORMAT_AC3_LE || format == AF_FORMAT_AC3_BE;
}

int af_format_is_iec61937(enum AFFormat format)
{
    return format == AF_FORMAT_IEC61937_LE || format == AF_FORMAT_IEC61937_BE;
}

int af_fmt2bits(enum AFFormat format)
{
    return sample_fmt_info[format].bits;
}

int af_bits2fmt(int bits)
{
    return (bits / 8 - 1) << 3;
}

const struct af_fmt_entry af_fmtstr_table[] = {
    { "mulaw", AF_FORMAT_MU_LAW },
    { "alaw",  AF_FORMAT_A_LAW },
    { "mpeg2", AF_FORMAT_MPEG2 },
    { "ac3le", AF_FORMAT_AC3_LE },
    { "ac3be", AF_FORMAT_AC3_BE },
    { "ac3",   AF_FORMAT_AC3 },
    { "iec61937le", AF_FORMAT_IEC61937_LE },
    { "iec61937be", AF_FORMAT_IEC61937_BE },
    { "iec61937", AF_FORMAT_IEC61937 },
    { "imaadpcm", AF_FORMAT_IMA_ADPCM },

    { "u8",     AF_FORMAT_U8 },
    { "s8",     AF_FORMAT_S8 },
    { "u16le",  AF_FORMAT_U16_LE },
    { "u16be",  AF_FORMAT_U16_BE },
    { "u16",    AF_FORMAT_U16 },
    { "s16le",  AF_FORMAT_S16_LE },
    { "s16be",  AF_FORMAT_S16_BE },
    { "s16",    AF_FORMAT_S16 },
    { "u24le",  AF_FORMAT_U24_LE },
    { "u24be",  AF_FORMAT_U24_BE },
    { "u24",    AF_FORMAT_U24 },
    { "s24le",  AF_FORMAT_S24_LE },
    { "s24be",  AF_FORMAT_S24_BE },
    { "s24",    AF_FORMAT_S24 },
    { "u32le",  AF_FORMAT_U32_LE },
    { "u32be",  AF_FORMAT_U32_BE },
    { "u32",    AF_FORMAT_U32 },
    { "s32le",  AF_FORMAT_S32_LE },
    { "s32be",  AF_FORMAT_S32_BE },
    { "s32ne",  AF_FORMAT_S32 },
    { "fltle",  AF_FORMAT_FLT_LE },
    { "fltbe",  AF_FORMAT_FLT_BE },
    { "flt",    AF_FORMAT_FLT },

    { "u8p",    AF_FORMAT_U8P },
    { "s8p",    AF_FORMAT_S8P },
    { "u16le",  AF_FORMAT_U16P_LE },
    { "u16be",  AF_FORMAT_U16P_BE },
    { "u16p",   AF_FORMAT_U16P },
    { "s16ple", AF_FORMAT_S16P_LE },
    { "s16pbe", AF_FORMAT_S16P_BE },
    { "s16p",   AF_FORMAT_S16P },
    { "u24ple", AF_FORMAT_U24P_LE },
    { "u24pbe", AF_FORMAT_U24P_BE },
    { "u24p",   AF_FORMAT_U24P },
    { "s24ple", AF_FORMAT_S24P_LE },
    { "s24pbe", AF_FORMAT_S24P_BE },
    { "s24p",   AF_FORMAT_S24P },
    { "u32ple", AF_FORMAT_U32P_LE },
    { "u32pbe", AF_FORMAT_U32P_BE },
    { "u32p",   AF_FORMAT_U32P },
    { "s32ple", AF_FORMAT_S32P_LE },
    { "s32pbe", AF_FORMAT_S32P_BE },
    { "s32p",   AF_FORMAT_S32P },
    { "fltple", AF_FORMAT_FLTP_LE },
    { "fltbe",  AF_FORMAT_FLTP_BE },
    { "fltp",   AF_FORMAT_FLTP },

    {0}
};

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
