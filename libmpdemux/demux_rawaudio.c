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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "m_option.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "libaf/format.h"


static int channels = 2;
static int samplerate = 44100;
static int format = AF_FORMAT_S16;

const m_option_t demux_rawaudio_opts[] = {
  { "channels", &channels, CONF_TYPE_INT,CONF_RANGE,1,8, NULL },
  { "rate", &samplerate, CONF_TYPE_INT,CONF_RANGE,1000,8*48000, NULL },
  { "format", &format, CONF_TYPE_AFMT, 0, 0, 0, NULL },
  {NULL, NULL, 0, 0, 0, 0, NULL}
};


static demuxer_t* demux_rawaudio_open(demuxer_t* demuxer) {
  sh_audio_t* sh_audio;
  WAVEFORMATEX* w;

  if (af_format_is_special(format))
      return NULL;

  sh_audio = new_sh_audio(demuxer,0);
  sh_audio->wf = w = malloc(sizeof(*w));
  // Not a WAVEFORMATEX format; just abuse it to pass the internal mplayer
  // format to ad_pcm.c
  w->wFormatTag = format;
  sh_audio->format = MKTAG('M', 'P', 'a', 'f');
  w->nChannels = sh_audio->channels = channels;
  w->nSamplesPerSec = sh_audio->samplerate = samplerate;
  sh_audio->samplesize = (af_fmt2bits(format) + 7) / 8;
  w->nAvgBytesPerSec = samplerate * sh_audio->samplesize * channels;
  w->nBlockAlign = channels * sh_audio->samplesize;
  w->wBitsPerSample = 8 * sh_audio->samplesize;
  w->cbSize = 0;

  demuxer->movi_start = demuxer->stream->start_pos;
  demuxer->movi_end = demuxer->stream->end_pos;

  demuxer->audio->id = 0;
  demuxer->audio->sh = sh_audio;
  sh_audio->ds = demuxer->audio;
  sh_audio->needs_parsing = 1;

  return demuxer;
}

static int demux_rawaudio_fill_buffer(demuxer_t* demuxer, demux_stream_t *ds) {
  sh_audio_t* sh_audio = demuxer->audio->sh;
  int l = sh_audio->wf->nAvgBytesPerSec;
  off_t spos = stream_tell(demuxer->stream);
  demux_packet_t*  dp;

  if(demuxer->stream->eof)
    return 0;

  dp = new_demux_packet(l);
  dp->pts = (spos - demuxer->movi_start)  / (float)(sh_audio->wf->nAvgBytesPerSec);
  dp->pos = (spos - demuxer->movi_start);

  l = stream_read(demuxer->stream,dp->buffer,l);
  resize_demux_packet(dp, l);
  ds_add_packet(ds,dp);

  return 1;
}

static void demux_rawaudio_seek(demuxer_t *demuxer,float rel_seek_secs,float audio_delay,int flags){
  stream_t* s = demuxer->stream;
  sh_audio_t* sh_audio = demuxer->audio->sh;
  off_t base,pos;

  base = (flags & SEEK_ABSOLUTE) ? demuxer->movi_start : stream_tell(s);
  if(flags & SEEK_FACTOR)
    pos = base + ((demuxer->movi_end - demuxer->movi_start)*rel_seek_secs);
  else
    pos = base + (rel_seek_secs*sh_audio->i_bps);

  pos -= (pos % (sh_audio->channels * sh_audio->samplesize) );
  stream_seek(s,pos);
//  printf("demux_rawaudio: streamtell=%d\n",(int)stream_tell(demuxer->stream));
}

const demuxer_desc_t demuxer_desc_rawaudio = {
  "Raw audio demuxer",
  "rawaudio",
  "rawaudio",
  "?",
  "",
  DEMUXER_TYPE_RAWAUDIO,
  0, // no autodetect
  NULL,
  demux_rawaudio_fill_buffer,
  demux_rawaudio_open,
  NULL,
  demux_rawaudio_seek,
};
