/*
 * video encoding using libavformat
 * Copyright (C) 2010 Nicolas George <george@nsup.org>
 * Copyright (C) 2011 Rudolf Polzer <divVerent@xonotic.org>
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
#include "mpcommon.h"
#include "options.h"
#include "fmt-conversion.h"
#include "libmpcodecs/mp_image.h"
#include "libmpcodecs/vfcap.h"
#include "subopt-helper.h"
#include "talloc.h"
#include "video_out.h"

#include "encode_lavc.h"

#include "sub/sub.h"
#include "libvo/osd.h"

//for sws_getContextFromCmdLine_hq and mp_sws_set_colorspace
#include "libmpcodecs/vf_scale.h"
#include "libvo/csputils.h"
#include <libswscale/swscale.h>

struct priv {
    uint8_t *buffer;
    size_t buffer_size;
    AVStream *stream;
    int have_first_packet;

    int harddup;

    double lastpts;
    int64_t lastipts;
    int64_t lastframeipts;
    double expected_next_pts;
    mp_image_t *lastimg;
    int lastimg_wants_osd;
    int lastdisplaycount;

    AVRational worst_time_base;
    int worst_time_base_is_stream;

    struct mp_csp_details colorspace;
};

static int preinit(struct vo *vo, const char *arg)
{
    struct priv *vc;
    if (!encode_lavc_available(vo->encode_lavc_ctx)) {
        mp_msg(MSGT_ENCODE, MSGL_ERR,
               "vo-lavc: the option -o (output file) must be specified\n");
        return -1;
    }
    vo->priv = talloc_zero(vo, struct priv);
    vc = vo->priv;
    vc->harddup = vo->encode_lavc_ctx->options->harddup;
    vc->colorspace = (struct mp_csp_details) MP_CSP_DETAILS_DEFAULTS;
    return 0;
}

static void draw_image(struct vo *vo, mp_image_t *mpi, double pts);
static void uninit(struct vo *vo)
{
    struct priv *vc = vo->priv;
    if (!vc)
        return;

    if (vc->lastipts >= 0 && vc->stream)
        draw_image(vo, NULL, MP_NOPTS_VALUE);

    if (vc->lastimg) {
        // palette hack
        if (vc->lastimg->imgfmt == IMGFMT_RGB8
                || vc->lastimg->imgfmt == IMGFMT_BGR8)
            vc->lastimg->planes[1] = NULL;
        free_mp_image(vc->lastimg);
        vc->lastimg = NULL;
    }

    vo->priv = NULL;
}

static int config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t flags,
                  uint32_t format)
{
    struct priv *vc = vo->priv;
    enum PixelFormat pix_fmt = imgfmt2pixfmt(format);
    AVRational display_aspect_ratio, image_aspect_ratio;
    AVRational aspect;

    if (!vc)
        return -1;

    display_aspect_ratio.num = d_width;
    display_aspect_ratio.den = d_height;
    image_aspect_ratio.num = width;
    image_aspect_ratio.den = height;
    aspect = av_div_q(display_aspect_ratio, image_aspect_ratio);

    if (vc->stream) {
        /* NOTE:
         * in debug builds we get a "comparison between signed and unsigned"
         * warning here. We choose to ignore that; just because ffmpeg currently
         * uses a plain 'int' for these struct fields, it doesn't mean it always
         * will */
        if (width == vc->stream->codec->width &&
                height == vc->stream->codec->height) {
            if (aspect.num != vc->stream->codec->sample_aspect_ratio.num ||
                    aspect.den != vc->stream->codec->sample_aspect_ratio.den) {
                /* aspect-only changes are not critical */
                mp_msg(MSGT_ENCODE, MSGL_WARN, "vo-lavc: unsupported pixel aspect "
                       "ratio change from %d:%d to %d:%d\n",
                       vc->stream->codec->sample_aspect_ratio.num,
                       vc->stream->codec->sample_aspect_ratio.den,
                       aspect.num, aspect.den);
            }
            return 0;
        }

        /* FIXME Is it possible with raw video? */
        mp_msg(MSGT_ENCODE, MSGL_ERR,
               "vo-lavc: resolution changes not supported.\n");
        goto error;
    }

    vc->lastipts = MP_NOPTS_VALUE;
    vc->lastframeipts = MP_NOPTS_VALUE;

    if (pix_fmt == PIX_FMT_NONE)
        goto error;  /* imgfmt2pixfmt already prints something */

    vc->stream = encode_lavc_alloc_stream(vo->encode_lavc_ctx,
                                          AVMEDIA_TYPE_VIDEO);
    vc->stream->sample_aspect_ratio = vc->stream->codec->sample_aspect_ratio =
            aspect;
    vc->stream->codec->width = width;
    vc->stream->codec->height = height;
    vc->stream->codec->pix_fmt = pix_fmt;

    encode_lavc_set_csp(vo->encode_lavc_ctx, vc->stream, vc->colorspace.format);
    encode_lavc_set_csp_levels(vo->encode_lavc_ctx, vc->stream, vc->colorspace.levels_out);

    if (encode_lavc_open_codec(vo->encode_lavc_ctx, vc->stream) < 0)
        goto error;

    vc->colorspace.format = encode_lavc_get_csp(vo->encode_lavc_ctx, vc->stream);
    vc->colorspace.levels_out = encode_lavc_get_csp_levels(vo->encode_lavc_ctx, vc->stream);

    vc->buffer_size = 6 * width * height + 200;
    if (vc->buffer_size < FF_MIN_BUFFER_SIZE)
        vc->buffer_size = FF_MIN_BUFFER_SIZE;
    if (vc->buffer_size < sizeof(AVPicture))
        vc->buffer_size = sizeof(AVPicture);

    vc->buffer = talloc_size(vc, vc->buffer_size);

    vc->lastimg = alloc_mpi(width, height, format);

    // palette hack
    if (vc->lastimg->imgfmt == IMGFMT_RGB8 ||
            vc->lastimg->imgfmt == IMGFMT_BGR8)
        vc->lastimg->planes[1] = talloc_zero_size(vc, 1024);

    return 0;

error:
    uninit(vo);
    return -1;
}

static int query_format(struct vo *vo, uint32_t format)
{
    enum PixelFormat pix_fmt = imgfmt2pixfmt(format);

    if (!vo->encode_lavc_ctx)
        return 0;

    if (!encode_lavc_supports_pixfmt(vo->encode_lavc_ctx, pix_fmt))
        return 0;

    return
        VFCAP_CSP_SUPPORTED |
            // we can do it
        VFCAP_CSP_SUPPORTED_BY_HW |
            // we don't convert colorspaces here (TODO: if we add EOSD rendering, only set this flag if EOSD can be rendered without extra conversions)
        VFCAP_OSD | VFCAP_EOSD | VFCAP_EOSD_RGBA |
            // we have OSD
        VOCAP_NOSLICES;
            // we don't use slices
}

static void write_packet(struct vo *vo, int size, AVPacket *packet)
{
    struct priv *vc = vo->priv;

    if (size < 0) {
        mp_msg(MSGT_ENCODE, MSGL_ERR, "vo-lavc: error encoding\n");
        return;
    }

    if (size > 0) {
        packet->stream_index = vc->stream->index;
        if (packet->pts != AV_NOPTS_VALUE) {
            packet->pts = av_rescale_q(packet->pts,
                                       vc->stream->codec->time_base,
                                       vc->stream->time_base);
        } else {
            mp_msg(MSGT_ENCODE, MSGL_WARN, "vo-lavc: codec did not provide pts\n");
            packet->pts = av_rescale_q(vc->lastipts, vc->worst_time_base,
                                       vc->stream->time_base);
        }
        if (packet->dts != AV_NOPTS_VALUE) {
            packet->dts = av_rescale_q(packet->dts,
                                       vc->stream->codec->time_base,
                                       vc->stream->time_base);
        }
        if (packet->duration > 0) {
            packet->duration = av_rescale_q(packet->duration,
                                       vc->stream->codec->time_base,
                                       vc->stream->time_base);
        } else {
            // HACK: libavformat calculates dts wrong if the initial packet
            // duration is not set, but ONLY if the time base is "high" and if we
            // have b-frames!
            if (!packet->duration)
                if (!vc->have_first_packet)
                    if (vc->stream->codec->has_b_frames
                            || vc->stream->codec->max_b_frames)
                        if (vc->stream->time_base.num * 1000LL <=
                                vc->stream->time_base.den)
                            packet->duration = FFMAX(1, av_rescale_q(1,
                                 vc->stream->codec->time_base, vc->stream->time_base));
        }

        if (encode_lavc_write_frame(vo->encode_lavc_ctx, packet) < 0) {
            mp_msg(MSGT_ENCODE, MSGL_ERR, "vo-lavc: error writing\n");
            return;
        }

        vc->have_first_packet = 1;
    }
}

static int encode_video(struct vo *vo, AVFrame *frame, AVPacket *packet)
{
    struct priv *vc = vo->priv;
    if (encode_lavc_oformat_flags(vo->encode_lavc_ctx) & AVFMT_RAWPICTURE) {
        if (!frame)
            return 0;
        memcpy(vc->buffer, frame, sizeof(AVPicture));
        mp_msg(MSGT_ENCODE, MSGL_DBG2, "vo-lavc: got pts %f\n",
               frame->pts * (double) vc->stream->codec->time_base.num /
                            (double) vc->stream->codec->time_base.den);
        packet->size = sizeof(AVPicture);
        return packet->size;
    } else {
        int got_packet = 0;
        int status = avcodec_encode_video2(vc->stream->codec, packet,
                                           frame, &got_packet);
        int size = (status < 0) ? status : got_packet ? packet->size : 0;

        if (frame)
            mp_msg(MSGT_ENCODE, MSGL_DBG2, "vo-lavc: got pts %f; out size: %d\n",
                   frame->pts * (double) vc->stream->codec->time_base.num /
                   (double) vc->stream->codec->time_base.den, size);

        encode_lavc_write_stats(vo->encode_lavc_ctx, vc->stream);
        return size;
    }
}

static void draw_image(struct vo *vo, mp_image_t *mpi, double pts)
{
    struct priv *vc = vo->priv;
    struct encode_lavc_context *ectx = vo->encode_lavc_ctx;
    int i, size;
    AVFrame *frame;
    AVCodecContext *avc;
    int64_t frameipts;
    double nextpts;

    if (!vc)
        return;
    if (!encode_lavc_start(ectx)) {
        mp_msg(MSGT_ENCODE, MSGL_WARN, "vo-lavc: NOTE: skipped initial video frame (probably because audio is not there yet)\n");
        return;
    }
    if (pts == MP_NOPTS_VALUE) {
        if (mpi)
            mp_msg(MSGT_ENCODE, MSGL_WARN, "vo-lavc: frame without pts, please report; synthesizing pts instead\n");
        pts = vc->expected_next_pts;
    }

    avc = vc->stream->codec;

    if (vc->worst_time_base.den == 0) {
        //if (avc->time_base.num / avc->time_base.den >= vc->stream->time_base.num / vc->stream->time_base.den)
        if (avc->time_base.num * (double) vc->stream->time_base.den >=
                vc->stream->time_base.num * (double) avc->time_base.den) {
            mp_msg(MSGT_ENCODE, MSGL_V, "vo-lavc: NOTE: using codec time base "
                   "(%d/%d) for frame dropping; the stream base (%d/%d) is "
                   "not worse.\n", (int)avc->time_base.num,
                   (int)avc->time_base.den, (int)vc->stream->time_base.num,
                   (int)vc->stream->time_base.den);
            vc->worst_time_base = avc->time_base;
            vc->worst_time_base_is_stream = 0;
        } else {
            mp_msg(MSGT_ENCODE, MSGL_WARN, "vo-lavc: NOTE: not using codec time "
                   "base (%d/%d) for frame dropping; the stream base (%d/%d) "
                   "is worse.\n", (int)avc->time_base.num,
                   (int)avc->time_base.den, (int)vc->stream->time_base.num,
                   (int)vc->stream->time_base.den);
            vc->worst_time_base = vc->stream->time_base;
            vc->worst_time_base_is_stream = 1;
        }

        // NOTE: we use the following "axiom" of av_rescale_q:
        // if time base A is worse than time base B, then
        //   av_rescale_q(av_rescale_q(x, A, B), B, A) == x
        // this can be proven as long as av_rescale_q rounds to nearest, which
        // it currently does

        // av_rescale_q(x, A, B) * B = "round x*A to nearest multiple of B"
        // and:
        //    av_rescale_q(av_rescale_q(x, A, B), B, A) * A
        // == "round av_rescale_q(x, A, B)*B to nearest multiple of A"
        // == "round 'round x*A to nearest multiple of B' to nearest multiple of A"
        //
        // assume this fails. Then there is a value of x*A, for which the
        // nearest multiple of B is outside the range [(x-0.5)*A, (x+0.5)*A[.
        // Absurd, as this range MUST contain at least one multiple of B.
    }

    double timeunit = (double)vc->worst_time_base.num / vc->worst_time_base.den;

    double outpts;
    if (ectx->options->rawts)
        outpts = pts;
    else if (ectx->options->copyts) {
        // fix the discontinuity pts offset
        nextpts = pts;
        if (ectx->discontinuity_pts_offset == MP_NOPTS_VALUE) {
            ectx->discontinuity_pts_offset = ectx->next_in_pts - nextpts;
        }
        else if (fabs(nextpts + ectx->discontinuity_pts_offset - ectx->next_in_pts) > 30) {
            mp_msg(MSGT_ENCODE, MSGL_WARN,
                    "vo-lavc: detected an unexpected discontinuity (pts jumped by "
                    "%f seconds)\n",
                    nextpts + ectx->discontinuity_pts_offset - ectx->next_in_pts);
            ectx->discontinuity_pts_offset = ectx->next_in_pts - nextpts;
        }

        outpts = pts + ectx->discontinuity_pts_offset;
    }
    else {
        // adjust pts by knowledge of audio pts vs audio playback time
        double duration = 0;
        if (ectx->last_video_in_pts != MP_NOPTS_VALUE)
            duration = pts - ectx->last_video_in_pts;
        if (duration < 0)
            duration = timeunit;   // XXX warn about discontinuity?
        outpts = vc->lastpts + duration;
        if (ectx->audio_pts_offset != MP_NOPTS_VALUE) {
            double adj = outpts - pts - ectx->audio_pts_offset;
            adj = FFMIN(adj, duration * 0.1);
            adj = FFMAX(adj, -duration * 0.1);
            outpts -= adj;
        }
    }
    vc->lastpts = outpts;
    ectx->last_video_in_pts = pts;
    frameipts = floor((outpts + encode_lavc_getoffset(ectx, vc->stream))
                      / timeunit + 0.5);

    // calculate expected pts of next video frame
    vc->expected_next_pts = pts + timeunit;

    if (!ectx->options->rawts && ectx->options->copyts) {
        // set next allowed output pts value
        nextpts = vc->expected_next_pts + ectx->discontinuity_pts_offset;
        if (nextpts > ectx->next_in_pts)
            ectx->next_in_pts = nextpts;
    }

    // never-drop mode
    if (ectx->options->neverdrop && frameipts <= vc->lastipts) {
        mp_msg(MSGT_ENCODE, MSGL_INFO, "vo-lavc: -oneverdrop increased pts by %d\n",
               (int) (vc->lastipts - frameipts + 1));
        frameipts = vc->lastipts + 1;
        vc->lastpts = frameipts * timeunit - encode_lavc_getoffset(ectx, vc->stream);
    }

    if (vc->lastipts != MP_NOPTS_VALUE) {
        frame = avcodec_alloc_frame();

        // we have a valid image in lastimg
        while (vc->lastipts < frameipts) {
            int64_t thisduration = vc->harddup ? 1 : (frameipts - vc->lastipts);
            AVPacket packet;

            avcodec_get_frame_defaults(frame);

            // this is a nop, unless the worst time base is the STREAM time base
            frame->pts = av_rescale_q(vc->lastipts, vc->worst_time_base,
                                      avc->time_base);

            for (i = 0; i < 4; i++) {
                frame->data[i] = vc->lastimg->planes[i];
                frame->linesize[i] = vc->lastimg->stride[i];
            }
            frame->quality = avc->global_quality;

            av_init_packet(&packet);
            packet.data = vc->buffer;
            packet.size = vc->buffer_size;
            size = encode_video(vo, frame, &packet);
            write_packet(vo, size, &packet);

            vc->lastipts += thisduration;
            ++vc->lastdisplaycount;
        }

        av_free(frame);
    }

    if (!mpi) {
        // finish encoding
        do {
            AVPacket packet;
            av_init_packet(&packet);
            packet.data = vc->buffer;
            packet.size = vc->buffer_size;
            size = encode_video(vo, NULL, &packet);
            write_packet(vo, size, &packet);
        } while (size > 0);
    } else {
        if (frameipts >= vc->lastframeipts) {
            if (vc->lastframeipts != MP_NOPTS_VALUE && vc->lastdisplaycount != 1)
                mp_msg(MSGT_ENCODE, MSGL_INFO,
                       "vo-lavc: Frame at pts %d got displayed %d times\n",
                       (int) vc->lastframeipts, vc->lastdisplaycount);
            copy_mpi(vc->lastimg, mpi);
            vc->lastimg_wants_osd = true;

            // palette hack
            if (vc->lastimg->imgfmt == IMGFMT_RGB8 ||
                    vc->lastimg->imgfmt == IMGFMT_BGR8)
                memcpy(vc->lastimg->planes[1], mpi->planes[1], 1024);

            vc->lastframeipts = vc->lastipts = frameipts;
            if (ectx->options->rawts && vc->lastipts < 0) {
                mp_msg(MSGT_ENCODE, MSGL_ERR, "vo-lavc: why does this happen? DEBUG THIS! vc->lastipts = %lld\n", (long long) vc->lastipts);
                vc->lastipts = -1;
            }
            vc->lastdisplaycount = 0;
        } else {
            mp_msg(MSGT_ENCODE, MSGL_INFO, "vo-lavc: Frame at pts %d got dropped "
                   "entirely because pts went backwards\n", (int) frameipts);
            vc->lastimg_wants_osd = false;
        }
    }
}

static void blend_const16_with_alpha(uint8_t *dst, ssize_t dstRowStride, uint8_t srcp, const uint8_t *srca, ssize_t srcaRowStride, uint8_t srcamul, int rows, int cols)
{
    int i, j;
    for (i = 0; i < rows; ++i) {
        uint16_t *dstr = (uint16_t *) (dst + dstRowStride * i);
        const uint8_t *srcar = srca + srcaRowStride * i;
        for (j = 0; j < cols; ++j) {
            uint16_t dstp = dstr[j];
            uint32_t srcap = srcar[j]; // 32bit to force the math ops to operate on 32 bit
            srcap = (srcap * srcamul / 255);
            uint16_t outp = (srcp * srcap * 257 + dstp * (255 - srcap)) / 255;
            dstr[j] = outp;
        }
    }
}

static void blend_src16_with_alpha(uint8_t *dst, ssize_t dstRowStride, const uint8_t *src, ssize_t srcRowStride, const uint8_t *srca, ssize_t srcaRowStride, uint8_t srcamul, int rows, int cols)
{
    int i, j;
    for (i = 0; i < rows; ++i) {
        uint16_t *dstr = (uint16_t *) (dst + dstRowStride * i);
        const uint16_t *srcr = (const uint16_t *) (src + srcRowStride * i);
        const uint8_t *srcar = srca + srcaRowStride * i;
        for (j = 0; j < cols; ++j) {
            uint16_t dstp = dstr[j];
            uint16_t srcp = srcr[j];
            uint32_t srcap = srcar[j]; // 32bit to force the math ops to operate on 32 bit
            srcap = (srcap * srcamul / 255);
            // uint16_t outp = (srcp * srcap + dstp * (255 - srcap)) / 255; // separate alpha GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
            srcp = (srcp * srcamul / 255); // premultiply
            uint16_t outp = srcp * 257 + (dstp * (255 - srcap)) / 255; // premultiplied alpha GL_ONE GL_ONE_MINUS_SRC_ALPHA
            dstr[j] = outp;
        }
    }
}

static void blend_const8_with_alpha(uint8_t *dst, ssize_t dstRowStride, uint8_t srcp, const uint8_t *srca, ssize_t srcaRowStride, uint8_t srcamul, int rows, int cols)
{
    int i, j;
    for (i = 0; i < rows; ++i) {
        uint8_t *dstr = dst + dstRowStride * i;
        const uint8_t *srcar = srca + srcaRowStride * i;
        for (j = 0; j < cols; ++j) {
            uint8_t dstp = dstr[j];
            uint16_t srcap = srcar[j]; // 32bit to force the math ops to operate on 32 bit
            srcap = (srcap * srcamul / 255);
            uint8_t outp = (srcp * srcap + dstp * (255 - srcap)) / 255;
            dstr[j] = outp;
        }
    }
}

static void blend_src8_with_alpha(uint8_t *dst, ssize_t dstRowStride, const uint8_t *src, ssize_t srcRowStride, const uint8_t *srca, ssize_t srcaRowStride, uint8_t srcamul, int rows, int cols)
{
    int i, j;
    for (i = 0; i < rows; ++i) {
        uint8_t *dstr = dst + dstRowStride * i;
        const uint8_t *srcr = src + srcRowStride * i;
        const uint8_t *srcar = srca + srcaRowStride * i;
        for (j = 0; j < cols; ++j) {
            uint8_t dstp = dstr[j];
            uint8_t srcp = srcr[j];
            uint16_t srcap = srcar[j]; // 32bit to force the math ops to operate on 32 bit
            srcap = (srcap * srcamul / 255);
            // uint8_t outp = (srcp * srcap + dstp * (255 - srcap)) / 255; // separate alpha GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
            srcp = (srcp * srcamul / 255); // premultiply
            uint8_t outp = srcp + (dstp * (255 - srcap)) / 255; // premultiplied alpha GL_ONE GL_ONE_MINUS_SRC_ALPHA
            dstr[j] = outp;
        }
    }
}

static void blend_with_alpha(uint8_t *dst, ssize_t dstRowStride, const uint8_t *src, ssize_t srcRowStride, uint8_t srcp, const uint8_t *srca, ssize_t srcaRowStride, uint8_t srcamul, int rows, int cols, int bytes)
{
    if (bytes == 2) {
        if (src)
            blend_src16_with_alpha(dst, dstRowStride, src, srcRowStride, srca, srcaRowStride, srcamul, rows, cols);
        else
            blend_const16_with_alpha(dst, dstRowStride, srcp, srca, srcaRowStride, srcamul, rows, cols);
    } else if (bytes == 1) {
        if (src)
            blend_src8_with_alpha(dst, dstRowStride, src, srcRowStride, srca, srcaRowStride, srcamul, rows, cols);
        else
            blend_const8_with_alpha(dst, dstRowStride, srcp, srca, srcaRowStride, srcamul, rows, cols);
    }
}

static void region_to_region(mp_image_t *dst, int dstRow, int dstRows, int dstRowStep, const mp_image_t *src, int srcRow, int srcRows, int srcRowStep, struct mp_csp_details *csp)
{
    int src_chroma_y_shift = src->chroma_y_shift == 31 ? 0 : src->chroma_y_shift;
    int dst_chroma_y_shift = dst->chroma_y_shift == 31 ? 0 : dst->chroma_y_shift;
    int mask = ((1 << dst_chroma_y_shift) - 1) | ((1 << src_chroma_y_shift) - 1);
    if ((dstRow | dstRows | srcRow | srcRows) & mask) {
        mp_msg(MSGT_VO, MSGL_ERR, "region_to_region: chroma y shift: cannot copy src row %d length %d to dst row %d length %d without problems, the output image may be corrupted (%d, %d, %d)\n", srcRow, srcRows, dstRow, dstRows, mask, dst->chroma_y_shift, src->chroma_y_shift);
    }
    struct SwsContext *sws = sws_getContextFromCmdLine_hq(src->w, srcRows, src->imgfmt, dst->w, dstRows, dst->imgfmt);
    struct mp_csp_details colorspace = MP_CSP_DETAILS_DEFAULTS;
    if (csp)
        colorspace = *csp;
    mp_sws_set_colorspace(sws, &colorspace);
    const uint8_t *const src_planes[4] = {
        src->planes[0] + srcRow * src->stride[0],
        src->planes[1] + (srcRow >> src_chroma_y_shift) * src->stride[1],
        src->planes[2] + (srcRow >> src_chroma_y_shift) * src->stride[2],
        src->planes[3] + (srcRow >> src_chroma_y_shift) * src->stride[3]
    };
    uint8_t *const dst_planes[4] = {
        dst->planes[0] + dstRow * dst->stride[0],
        dst->planes[1] + (dstRow >> dst_chroma_y_shift) * dst->stride[1],
        dst->planes[2] + (dstRow >> dst_chroma_y_shift) * dst->stride[2],
        dst->planes[3] + (dstRow >> dst_chroma_y_shift) * dst->stride[3]
    };
    const int src_stride[4] = {
        src->stride[0] * srcRowStep,
        src->stride[1] * srcRowStep,
        src->stride[2] * srcRowStep,
        src->stride[3] * srcRowStep
    };
    const int dst_stride[4] = {
        dst->stride[0] * dstRowStep,
        dst->stride[1] * dstRowStep,
        dst->stride[2] * dstRowStep,
        dst->stride[3] * dstRowStep
    };
    sws_scale(sws, src_planes, src_stride, 0, srcRows, dst_planes, dst_stride);
    sws_freeContext(sws);
}

#define MP_MAP_YUV2RGB_COLOR(m,y,u,v,c) ((m)[c][0] * (y) + (m)[c][1] * (u) + (m)[c][2] * (v) + (m)[c][3])
#define MP_MAP_RGB2YUV_COLOR(minv,r,g,b,c) ((minv)[c][0] * (r) + (minv)[c][1] * (g) + (minv)[c][2] * (b) + (minv)[c][3])
static void mp_invert_yuv2rgb(float out[3][4], float in[3][4])
{
    float det;

    // this seems to help gcc's common subexpression elimination, and also makes the code look nicer
    float   m00 = in[0][0], m01 = in[0][1], m02 = in[0][2], m03 = in[0][3],
            m10 = in[1][0], m11 = in[1][1], m12 = in[1][2], m13 = in[1][3],
            m20 = in[2][0], m21 = in[2][1], m22 = in[2][2], m23 = in[2][3];

    // calculate the adjoint
    out[0][0] =  (m11*m22 - m21*m12);
    out[0][1] = -(m01*m22 - m21*m02);
    out[0][2] =  (m01*m12 - m11*m02);
    out[1][0] = -(m10*m22 - m20*m12);
    out[1][1] =  (m00*m22 - m20*m02);
    out[1][2] = -(m00*m12 - m10*m02);
    out[2][0] =  (m10*m21 - m20*m11);
    out[2][1] = -(m00*m21 - m20*m01);
    out[2][2] =  (m00*m11 - m10*m01);

    // calculate the determinant (as inverse == 1/det * adjoint, adjoint * m == identity * det, so this calculates the det)
    det = m00*out[0][0] + m10*out[0][1] + m20*out[0][2];
    if (det == 0.0f) {
        mp_msg(MSGT_VO, MSGL_ERR, "cannot invert yuv2rgb matrix\n");
        return;
    }

    // multiplications are faster than divisions, usually
    det = 1.0f / det;

    // manually unrolled loop to multiply all matrix elements by 1/det
    out[0][0] *= det; out[0][1] *= det; out[0][2] *= det;
    out[1][0] *= det; out[1][1] *= det; out[1][2] *= det;
    out[2][0] *= det; out[2][1] *= det; out[2][2] *= det;

    // fix the constant coefficient
    // rgb = M * yuv + C
    // M^-1 * rgb = yuv + M^-1 * C
    // yuv = M^-1 * rgb - M^-1 * C
    //                  ^^^^^^^^^^
    out[0][3] = -(out[0][0] * m03 + out[0][1] * m13 + out[0][2] * m23);
    out[1][3] = -(out[1][0] * m03 + out[1][1] * m13 + out[1][2] * m23);
    out[2][3] = -(out[2][0] * m03 + out[2][1] * m13 + out[2][2] * m23);
}

static void render_sub_bitmap(mp_image_t *dst, struct sub_bitmaps *sbs, struct mp_csp_details *csp)
{
    int i, x, y;
    int firstRow = dst->h;
    int endRow = 0;
    int color_yuv[4];
    float yuv2rgb[3][4];
    float rgb2yuv[3][4];
    struct mp_csp_params cspar = { .colorspace = *csp, .brightness = 0, .contrast = 1, .hue = 0, .saturation = 1, .rgamma = 1, .ggamma = 1, .bgamma = 1, .texture_bits = 8, .input_bits = 8 };
#if 1
    int format = IMGFMT_444P16;
    int bytes = 2;
#else
    int format = IMGFMT_444P;
    int bytes = 1;
#endif

    // prepare YUV/RGB conversion values
    mp_get_yuv2rgb_coeffs(&cspar, yuv2rgb);
    mp_invert_yuv2rgb(rgb2yuv, yuv2rgb);

    // calculate bounding range
    for (i = 0; i < sbs->num_parts; ++i) {
        struct sub_bitmap *sb = &sbs->parts[i];
        if (sb->y < firstRow)
            firstRow = sb->y;
        if (sb->y + sb->dh > endRow)
            endRow = sb->y + sb->dh;
    }
    firstRow &= ~((1 << dst->chroma_y_shift) - 1);
    --endRow;
    endRow |= (1 << dst->chroma_y_shift) - 1;
    ++endRow;

    if (firstRow < 0)
        firstRow = 0;
    if (endRow > dst->h)
        endRow = dst->h;
    if (firstRow >= endRow)
        return; // nothing to do

    // allocate temp image
    mp_image_t *temp = alloc_mpi(dst->w, endRow - firstRow, format);

    // convert to temp image
    region_to_region(temp, 0, endRow - firstRow, 1, dst, firstRow, endRow - firstRow, 1, csp);

    for (i = 0; i < sbs->num_parts; ++i) {
        struct sub_bitmap *sb = &sbs->parts[i];
        mp_image_t *sbi = NULL;
        mp_image_t *sba = NULL;

        // cut off areas outside the image
        int dst_x = sb->x;
        int dst_y = sb->y;
        int dst_w = sb->dw;
        int dst_h = sb->dh;
        if (dst_x < 0) {
            dst_w += dst_x;
            dst_x = 0;
        }
        if (dst_y < 0) {
            dst_h += dst_y;
            dst_y = 0;
        }
        if (dst_x + dst_w > dst->w) {
            dst_w = dst->w - dst_x;
        }
        if (dst_y + dst_h > dst->h) {
            dst_h = dst->h - dst_y;
        }

        // return if nothing left
        if (dst_w <= 0 || dst_h <= 0)
            continue;

        if (sbs->format == SUBBITMAP_RGBA && sb->w >= 8) { // >= 8 because of libswscale madness
            // swscale the bitmap from w*h to dw*dh, changing BGRA8 into YUV444P16 and make a scaled copy of A8
            mp_image_t *sbisrc = new_mp_image(sb->w, sb->h);
            mp_image_setfmt(sbisrc, IMGFMT_BGRA);
            sbisrc->planes[0] = sb->bitmap;
            sbi = alloc_mpi(sb->dw, sb->dh, format);
            region_to_region(sbi, 0, sb->dh, 1, sbisrc, 0, sb->h, 1, csp);
            free_mp_image(sbisrc);

            mp_image_t *sbasrc = alloc_mpi(sb->w, sb->h, IMGFMT_Y8);
            for (y = 0; y < sb->h; ++y)
                for (x = 0; x < sb->w; ++x)
                    sbasrc->planes[0][x + y * sbasrc->stride[0]] = ((unsigned char *) sb->bitmap)[(x + y * sb->stride) * 4 + 3];
            sba = alloc_mpi(sb->dw, sb->dh, IMGFMT_Y8);
            region_to_region(sba, 0, sb->dh, 1, sbasrc, 0, sb->h, 1, csp);
            free_mp_image(sbasrc);
            memset(color_yuv, 0, sizeof(color_yuv));
            color_yuv[3] = 255;
        } else if (sbs->format == SUBBITMAP_LIBASS && !sbs->scaled) {
            // swscale alpha only
            sba = new_mp_image(sb->w, sb->h);
            mp_image_setfmt(sba, IMGFMT_Y8);
            sba->planes[0] = sb->bitmap;
            sba->stride[0] = sb->stride;
            int r = (sb->libass.color >> 24) & 0xFF;
            int g = (sb->libass.color >> 16) & 0xFF;
            int b = (sb->libass.color >> 8) & 0xFF;
            int a = sb->libass.color & 0xFF;
            color_yuv[0] = MP_MAP_RGB2YUV_COLOR(rgb2yuv, r / 255.0, g / 255.0, b / 255.0, 0) * 255.0;
            color_yuv[1] = MP_MAP_RGB2YUV_COLOR(rgb2yuv, r / 255.0, g / 255.0, b / 255.0, 1) * 255.0;
            color_yuv[2] = MP_MAP_RGB2YUV_COLOR(rgb2yuv, r / 255.0, g / 255.0, b / 255.0, 2) * 255.0;
            color_yuv[3] = 255 - a;
            // NOTE: these overflows can actually happen (when subtitles use color 0,0,0 while output levels only allows 16,16,16 upwards...)
            if(color_yuv[0] < 0)
                color_yuv[0] = 0;
            if(color_yuv[1] < 0)
                color_yuv[1] = 0;
            if(color_yuv[2] < 0)
                color_yuv[2] = 0;
            if(color_yuv[3] < 0)
                color_yuv[3] = 0;
            if(color_yuv[0] > 255)
                color_yuv[0] = 255;
            if(color_yuv[1] > 255)
                color_yuv[1] = 255;
            if(color_yuv[2] > 255)
                color_yuv[2] = 255;
            if(color_yuv[3] > 255)
                color_yuv[3] = 255;
        } else {
            mp_msg(MSGT_VO, MSGL_ERR, "render_sub_bitmap: invalid sub bitmap type\n");
            continue;
        }

        // call blend_with_alpha 3 times
        int p;
        for(p = 0; p < 1; ++p) // FIXME 3
            blend_with_alpha(
                    (temp->planes[p] + (dst_y - firstRow) * temp->stride[p]) + dst_x * bytes,
                    temp->stride[p],
                    sbi ? sbi->planes[p] + (dst_y - sb->y) * sbi->stride[p] + (dst_x - sb->x) * bytes : NULL,
                    sbi ? sbi->stride[p] : 0,
                    color_yuv[p],
                    sba->planes[0] + (dst_y - sb->y) * sba->stride[0] + (dst_x - sb->x),
                    sba->stride[0],
                    color_yuv[3],
                    dst_h, dst_w, bytes
                    );
    }

    // convert back
    region_to_region(dst, firstRow, endRow - firstRow, 1, temp, 0, endRow - firstRow, 1, csp);

    // clean up
    free_mp_image(temp);
}

// TODO wire EOSD rendering to render_sub_bitmap

static void flip_page_timed(struct vo *vo, unsigned int pts_us, int duration)
{
}

static void check_events(struct vo *vo)
{
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *vc = vo->priv;
    switch (request) {
    case VOCTRL_QUERY_FORMAT:
        return query_format(vo, *((uint32_t *)data));
    case VOCTRL_DRAW_IMAGE:
        draw_image(vo, (mp_image_t *)data, vo->next_pts);
        return 0;
    case VOCTRL_SET_YUV_COLORSPACE:
        vc->colorspace = *(struct mp_csp_details *)data;
        if (vc->stream) {
            encode_lavc_set_csp(vo->encode_lavc_ctx, vc->stream, vc->colorspace.format);
            encode_lavc_set_csp_levels(vo->encode_lavc_ctx, vc->stream, vc->colorspace.levels_out);
            vc->colorspace.format = encode_lavc_get_csp(vo->encode_lavc_ctx, vc->stream);
            vc->colorspace.levels_out = encode_lavc_get_csp_levels(vo->encode_lavc_ctx, vc->stream);
        }
        return 1;
    case VOCTRL_GET_YUV_COLORSPACE:
        *(struct mp_csp_details *)data = vc->colorspace;
        return 1;
    case VOCTRL_DRAW_EOSD:
        if (!data)
            return VO_FALSE;
        if (vc->lastimg && vc->lastimg_wants_osd) {
            struct sub_bitmaps *imgs = data;
            render_sub_bitmap(vc->lastimg, imgs, &vc->colorspace);
        }
        return VO_TRUE;
    case VOCTRL_GET_EOSD_RES: {
        struct mp_eosd_res *r = data;
        r->w = vc->stream->codec->width;
        r->h = vc->stream->codec->height;
        r->ml = r->mr = 0;
        r->mt = r->mb = 0;
        return VO_TRUE;
    }
    case VOCTRL_QUERY_EOSD_FORMAT: {
        int format = *(int *)data;
        return (format == SUBBITMAP_LIBASS || format == SUBBITMAP_RGBA)
               ? VO_TRUE : VO_NOTIMPL;
    }
    }
    return VO_NOTIMPL;
}

const struct vo_driver video_out_lavc = {
    .is_new = true,
    .buffer_frames = false,
    .info = &(const struct vo_info_s){
        "video encoding using libavcodec",
        "lavc",
        "Nicolas George <george@nsup.org>, Rudolf Polzer <divVerent@xonotic.org>",
        ""
    },
    .preinit = preinit,
    .config = config,
    .control = control,
    .uninit = uninit,
    .check_events = check_events,
    .draw_osd = draw_osd_with_eosd,
    .flip_page_timed = flip_page_timed,
};

// vim: sw=4 ts=4 et
