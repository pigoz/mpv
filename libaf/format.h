/*
 * The sample format system used lin libaf is based on bitmasks.
 * The format definition only refers to the storage format,
 * not the resolution.
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

#ifndef MPLAYER_AF_FORMAT_H
#define MPLAYER_AF_FORMAT_H

#include <sys/types.h>
#include "config.h"
#include "bstr.h"

#define DEF_FMT(x) x##_LE, x##_BE, x##P_LE, x##P_BE

enum AFFormat {
    AF_FORMAT_UNKNOWN,

    AF_FORMAT_U8,
    AF_FORMAT_U8P,
    AF_FORMAT_S8,
    AF_FORMAT_S8P,

    DEF_FMT(AF_FORMAT_U16),
    DEF_FMT(AF_FORMAT_S16),
    DEF_FMT(AF_FORMAT_U24),
    DEF_FMT(AF_FORMAT_S24),
    DEF_FMT(AF_FORMAT_U32),
    DEF_FMT(AF_FORMAT_S32),
    DEF_FMT(AF_FORMAT_FLT),

    AF_FORMAT_MU_LAW,
    AF_FORMAT_A_LAW,
    AF_FORMAT_MPEG2,          // MPEG(2) audio
    AF_FORMAT_AC3_LE,         // Dolby Digital AC3
    AF_FORMAT_AC3_BE,         // Dolby Digital AC3
    AF_FORMAT_IEC61937_LE,
    AF_FORMAT_IEC61937_BE,

    AF_FORMAT_IMA_ADPCM,

    AF_FORMAT_NB,
};

// the following is going to be ugly: be prepared

#if BYTE_ORDER == BIG_ENDIAN
#define AF_FORMAT_U16     AF_FORMAT_U16_BE
#define AF_FORMAT_S16     AF_FORMAT_S16_BE
#define AF_FORMAT_U24     AF_FORMAT_U24_BE
#define AF_FORMAT_S24     AF_FORMAT_S24_BE
#define AF_FORMAT_U32     AF_FORMAT_U32_BE
#define AF_FORMAT_S32     AF_FORMAT_S32_BE
#define AF_FORMAT_FLT     AF_FORMAT_FLT_BE

#define AF_FORMAT_U16P    AF_FORMAT_U16P_BE
#define AF_FORMAT_S16P    AF_FORMAT_S16P_BE
#define AF_FORMAT_U24P    AF_FORMAT_U24P_BE
#define AF_FORMAT_S24P    AF_FORMAT_S24P_BE
#define AF_FORMAT_U32P    AF_FORMAT_U32P_BE
#define AF_FORMAT_S32P    AF_FORMAT_S32P_BE
#define AF_FORMAT_FLTP    AF_FORMAT_FLTP_BE

#define AF_FORMAT_AC3       AF_FORMAT_AC3_BE
#define AF_FORMAT_IEC61937  AF_FORMAT_IEC61937_BE
#else
#define AF_FORMAT_U16     AF_FORMAT_U16_LE
#define AF_FORMAT_S16     AF_FORMAT_S16_LE
#define AF_FORMAT_U24     AF_FORMAT_U24_LE
#define AF_FORMAT_S24     AF_FORMAT_S24_LE
#define AF_FORMAT_U32     AF_FORMAT_U32_LE
#define AF_FORMAT_S32     AF_FORMAT_S32_LE
#define AF_FORMAT_FLT     AF_FORMAT_FLT_LE

#define AF_FORMAT_U16P    AF_FORMAT_U16P_LE
#define AF_FORMAT_S16P    AF_FORMAT_S16P_LE
#define AF_FORMAT_U24P    AF_FORMAT_U24P_LE
#define AF_FORMAT_S24P    AF_FORMAT_S24P_LE
#define AF_FORMAT_U32P    AF_FORMAT_U32P_LE
#define AF_FORMAT_S32P    AF_FORMAT_S32P_LE
#define AF_FORMAT_FLTP    AF_FORMAT_FLTP_LE

#define AF_FORMAT_AC3       AF_FORMAT_AC3_LE
#define AF_FORMAT_IEC61937  AF_FORMAT_IEC61937_LE
#endif

#undef DEF_FMT

int af_format_bits(enum AFFormat format);

int af_format_is_unsigned(enum AFFormat format);
int af_format_is_signed(enum AFFormat format);
enum AFFormat af_format_alternate(enum AFFormat format);

int af_format_is_integer(enum AFFormat format);
int af_format_is_float(enum AFFormat format);

int af_format_is_le(enum AFFormat format);
int af_format_is_be(enum AFFormat format);
int af_format_is_ne(enum AFFormat format); // here be ifdefs
enum AFFormat af_format_swapped(enum AFFormat format);

int af_format_is_planar(enum AFFormat format);
int af_format_is_special(enum AFFormat format);

int af_format_is_ac3(enum AFFormat format);
int af_format_is_iec61937(enum AFFormat format);

const char *af_format_name(enum AFFormat format);

struct af_fmt_entry {
    const char *name;
    int format;
};

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


int af_str2fmt_short(bstr str);
int af_fmt2bits(int format);
int af_bits2fmt(int bits);
char *af_fmt2str(int format, char *str, int size);
const char *af_fmt2str_short(int format);

#endif /* MPLAYER_AF_FORMAT_H */
