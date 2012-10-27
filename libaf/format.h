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

enum AFSampleFormat {
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
    AF_FORMAT_AC3_BE,         // Dolby Digital AC3
    AF_FORMAT_AC3_LE,         // Dolby Digital AC3
    AF_FORMAT_IMA_ADPCM,
    AF_FORMAT_IEC61937,

    AF_FORMAT_NB,
};

enum AFSampleEncoding {
    AF_PCM,
    AF_AC3,
    AF_IEC61937,
}

// the following is going to be ugly: be prepared

#if BYTE_ORDER == BIG_ENDIAN
#define AF_FORMAT_U16     AF_FORMAT_U16_BE
#define AF_FORMAT_S16     AF_FORMAT_S16_BE
#define AF_FORMAT_U24     AF_FORMAT_U24_BE
#define AF_FORMAT_S24     AF_FORMAT_S24_BE
#define AF_FORMAT_U32     AF_FORMAT_U32_BE
#define AF_FORMAT_S32     AF_FORMAT_S32_BE
#define AF_FORMAT_FLT     AF_FORMAT_FLT_BE
#define AF_FORMAT_AC3     AF_FORMAT_AC3_BE

#define AF_FORMAT_U16P    AF_FORMAT_U16P_BE
#define AF_FORMAT_S16P    AF_FORMAT_S16P_BE
#define AF_FORMAT_U24P    AF_FORMAT_U24P_BE
#define AF_FORMAT_S24P    AF_FORMAT_S24P_BE
#define AF_FORMAT_U32P    AF_FORMAT_U32P_BE
#define AF_FORMAT_S32P    AF_FORMAT_S32P_BE
#define AF_FORMAT_FLTP    AF_FORMAT_FLTP_BE
#define AF_FORMAT_AC3P    AF_FORMAT_AC3P_BE
#else
#define AF_FORMAT_U16     AF_FORMAT_U16_LE
#define AF_FORMAT_S16     AF_FORMAT_S16_LE
#define AF_FORMAT_U24     AF_FORMAT_U24_LE
#define AF_FORMAT_S24     AF_FORMAT_S24_LE
#define AF_FORMAT_U32     AF_FORMAT_U32_LE
#define AF_FORMAT_S32     AF_FORMAT_S32_LE
#define AF_FORMAT_FLT     AF_FORMAT_FLT_LE
#define AF_FORMAT_AC3     AF_FORMAT_AC3_LE

#define AF_FORMAT_U16P    AF_FORMAT_U16P_LE
#define AF_FORMAT_S16P    AF_FORMAT_S16P_LE
#define AF_FORMAT_U24P    AF_FORMAT_U24P_LE
#define AF_FORMAT_S24P    AF_FORMAT_S24P_LE
#define AF_FORMAT_U32P    AF_FORMAT_U32P_LE
#define AF_FORMAT_S32P    AF_FORMAT_S32P_LE
#define AF_FORMAT_FLTP    AF_FORMAT_FLTP_LE
#define AF_FORMAT_AC3P    AF_FORMAT_AC3P_LE
#endif

#undef DEF_FMT

int af_format_bits(enum AFSampleFormat format);
int af_format_us(enum AFSampleFormat format);
int af_format_le(enum AFSampleFormat format);
int af_format_be(enum AFSampleFormat format);
int af_format_planar(enum AFSampleFormat format);
int af_format_special(enum AFSampleFormat format);

enum AFSampleEncoding af_format_encoding(enum AFSampleFormat format);
int af_format_ac3(enum AFSampleFormat format);
int af_format_iec61937(enum AFSampleFormat format);

const char *af_format_name(enum AFSampleFormat format);

//// Signed/unsigned
//#define AF_FORMAT_SI            (0 << 1) // Signed
//#define AF_FORMAT_US            (1 << 1) // Unsigned
//#define AF_FORMAT_SIGN_MASK     (1 << 1)
//
//// Fixed or floating point
//#define AF_FORMAT_I             (0 << 2) // Int
//#define AF_FORMAT_F             (1 << 2) // Foating point
//#define AF_FORMAT_POINT_MASK    (1 << 2)
//
//// Bits used
//#define AF_FORMAT_8BIT          (0 << 3)
//#define AF_FORMAT_16BIT         (1 << 3)
//#define AF_FORMAT_24BIT         (2 << 3)
//#define AF_FORMAT_32BIT         (3 << 3)
//#define AF_FORMAT_40BIT         (4 << 3)
//#define AF_FORMAT_48BIT         (5 << 3)
//#define AF_FORMAT_BITS_MASK     (7 << 3)
//
//// Special flags refering to non pcm data
//#define AF_FORMAT_MU_LAW        (1 << 6)
//#define AF_FORMAT_A_LAW         (2 << 6)
//#define AF_FORMAT_MPEG2         (3 << 6) // MPEG(2) audio
//#define AF_FORMAT_AC3           (4 << 6) // Dolby Digital AC3
//#define AF_FORMAT_IMA_ADPCM     (5 << 6)
//#define AF_FORMAT_IEC61937      (6 << 6)
//#define AF_FORMAT_SPECIAL_MASK  (7 << 6)
//
//#define AF_FORMAT_MASK          ((1 << 9) - 1)

// PREDEFINED formats

//#define AF_FORMAT_UNKNOWN (-1)
//
//#define AF_FORMAT_IS_AC3(fmt) (((fmt) & AF_FORMAT_SPECIAL_MASK) == \
//                               AF_FORMAT_AC3)
//#define AF_FORMAT_IS_IEC61937(fmt) (((fmt) & AF_FORMAT_SPECIAL_MASK) == \
                                    AF_FORMAT_IEC61937)
int af_str2fmt_short(bstr str);
int af_fmt2bits(int format);
int af_bits2fmt(int bits);
char *af_fmt2str(int format, char *str, int size);
const char *af_fmt2str_short(int format);

#endif /* MPLAYER_AF_FORMAT_H */
