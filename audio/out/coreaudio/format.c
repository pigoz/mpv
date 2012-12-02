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

#include "format.h"

void ca_print_asbd(int log_level, const char* message,
                   const AudioStreamBasicDescription *asbd){
    uint32_t flags = asbd->mFormatFlags;

    // the following sucks but is quite useful for debugging...
    ca_msg(
        log_level,
        "%s %7.2fHz %dbit [%c%c%c%c][%"PRIu32"]"
        "[%"PRIu32"][%"PRIu32"][%"PRIu32"][%"PRIu32"] %s %s %s%s%s%s\n",
        message, asbd->mSampleRate, asbd->mBitsPerChannel,
        (int)(asbd->mFormatID & 0xff000000) >> 24,
        (int)(asbd->mFormatID & 0x00ff0000) >> 16,
        (int)(asbd->mFormatID & 0x0000ff00) >>  8,
        (int)(asbd->mFormatID & 0x000000ff) >>  0,
        asbd->mFormatFlags, asbd->mBytesPerPacket,
        asbd->mFramesPerPacket, asbd->mBytesPerFrame,
        asbd->mChannelsPerFrame,
        (flags&kAudioFormatFlagIsFloat) ? "float" : "int",
        (flags&kAudioFormatFlagIsBigEndian) ? "BE" : "LE",
        (flags&kAudioFormatFlagIsSignedInteger) ? "S" : "U",
        (flags&kAudioFormatFlagIsPacked) ? " packed" : "",
        (flags&kAudioFormatFlagIsAlignedHigh) ? " aligned" : "",
        (flags&kAudioFormatFlagIsNonInterleaved) ? " planar" : "" );
}

uint32_t ca_calc_asbd_lpcm_flags (
    bool is_float,
    bool is_signed_integer,
    bool is_big_endian,
    bool is_non_interleaved
) {
    // Cannot be float and signed integer at the same time
    assert(! (is_float && is_signed_integer));
    return kAudioFormatFlagIsPacked |
        (is_float           ? kAudioFormatFlagIsFloat          : 0) |
        (is_signed_integer  ? kAudioFormatFlagIsSignedInteger  : 0) |
        (is_big_endian      ? kAudioFormatFlagIsBigEndian      : 0) |
        (is_non_interleaved ? kAudioFormatFlagIsNonInterleaved : 0) ;
};

void ca_fillout_asbd_for_lpcm (
   AudioStreamBasicDescription *asbd,
   Float64  samplerate,
   uint32_t channels,
   uint32_t bits_per_channel,
   bool     is_float,
   bool     is_signed_integer,
   bool     is_big_endian,
   bool     is_non_interleaved
) {
    uint32_t bpf = (is_non_interleaved ? 1 : channels) * (bits_per_channel / 8);

    asbd->mSampleRate       = samplerate;
    asbd->mFormatID         = kAudioFormatLinearPCM;
    asbd->mBytesPerPacket   = bpf;
    asbd->mFramesPerPacket  = 1;
    asbd->mBytesPerFrame    = bpf;
    asbd->mChannelsPerFrame = channels;
    asbd->mBitsPerChannel   = bits_per_channel;
    asbd->mFormatFlags      = ca_calc_asbd_lpcm_flags(is_float,
                                                      is_signed_integer,
                                                      is_big_endian,
                                                      is_non_interleaved);
};

void ca_fillout_asbd_for_lpcm2 (
    AudioStreamBasicDescription *asbd,
    struct ao *ao
) {
    ca_fillout_asbd_for_lpcm(
        asbd,
        ao->samplerate,
        ao->channels,
        af_fmt2bits(ao->format),
        (ao->format & AF_FORMAT_POINT_MASK) == AF_FORMAT_F,
        (ao->format & AF_FORMAT_SIGN_MASK)  == AF_FORMAT_SI,
        (ao->format & AF_FORMAT_END_MASK)   == AF_FORMAT_BE,
        false // planar audio
    );
};
