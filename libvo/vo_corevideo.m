/*
 * CoreVideo video output driver
 * Copyright (c) 2005 Nicolas Plourde <nicolasplourde@gmail.com>
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

#include <assert.h>

#import "vo_corevideo.h"

// mplayer includes
#import "fastmemcpy.h"
#import "talloc.h"
#import "video_out.h"
#import "aspect.h"
#import "sub/sub.h"
#import "subopt-helper.h"

#import "csputils.h"
#import "libmpcodecs/vfcap.h"
#import "libmpcodecs/mp_image.h"

#import "gl_common.h"
#import "gl_osd.h"
#import "cocoa_common.h"

struct quad {
    GLfloat lowerLeft[2];
    GLfloat lowerRight[2];
    GLfloat upperRight[2];
    GLfloat upperLeft[2];
};

struct priv {
    MPGLContext *mpglctx;
    OSType pixelFormat;
    unsigned int image_width;
    unsigned int image_height;
    struct mp_csp_details colorspace;
    int ass_border_x, ass_border_y;

    CVPixelBufferRef pixelBuffer;
    CVOpenGLTextureCacheRef textureCache;
    CVOpenGLTextureRef texture;
    struct quad *quad;

    struct mpgl_osd *osd;
};

static void resize(struct vo *vo, int width, int height)
{
    struct priv *p = vo->priv;
    GL *gl = p->mpglctx->gl;

    gl->Viewport(0, 0, width, height);
    gl->MatrixMode(GL_PROJECTION);
    gl->LoadIdentity();
    p->ass_border_x = p->ass_border_y = 0;
    if (aspect_scaling()) {
        int new_w, new_h;
        GLdouble scale_x, scale_y;

        aspect(vo, &new_w, &new_h, A_WINZOOM);
        panscan_calc_windowed(vo);
        new_w += vo->panscan_x;
        new_h += vo->panscan_y;
        scale_x = (GLdouble)new_w / (GLdouble)width;
        scale_y = (GLdouble)new_h / (GLdouble)height;
        gl->Scaled(scale_x, scale_y, 1);
        p->ass_border_x = (vo->dwidth - new_w) / 2;
        p->ass_border_y = (vo->dheight - new_h) / 2;
    }

    gl->Ortho(0, p->image_width, p->image_height, 0, -1.0, 1.0);
    gl->MatrixMode(GL_MODELVIEW);
    gl->LoadIdentity();

    gl->Clear(GL_COLOR_BUFFER_BIT);
    vo->want_redraw = true;
}

static int init_gl(struct vo *vo, uint32_t d_width, uint32_t d_height)
{
    struct priv *p = vo->priv;
    GL *gl = p->mpglctx->gl;

    const char *vendor     = gl->GetString(GL_VENDOR);
    const char *version    = gl->GetString(GL_VERSION);
    const char *renderer   = gl->GetString(GL_RENDERER);

    mp_msg(MSGT_VO, MSGL_V, "[vo_corevideo] Running on OpenGL '%s' by '%s',"
           " version '%s'\n", renderer, vendor, version);

    gl->Disable(GL_BLEND);
    gl->Disable(GL_DEPTH_TEST);
    gl->DepthMask(GL_FALSE);
    gl->Disable(GL_CULL_FACE);
    gl->Enable(GL_TEXTURE_2D);
    gl->DrawBuffer(GL_BACK);
    gl->TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    if (!p->osd)
        p->osd = mpgl_osd_init(gl, true);

    resize(vo, d_width, d_height);

    gl->ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (gl->SwapInterval)
        gl->SwapInterval(1);
    return 1;
}

static void release_cv_entities(struct vo *vo) {
    struct priv *p = vo->priv;
    CVPixelBufferRelease(p->pixelBuffer);
    p->pixelBuffer = NULL;
    CVOpenGLTextureRelease(p->texture);
    p->texture = NULL;
    CVOpenGLTextureCacheRelease(p->textureCache);
    p->textureCache = NULL;
}

static int config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t flags,
                  uint32_t format)
{
    struct priv *p = vo->priv;
    release_cv_entities(vo);
    p->image_width = width;
    p->image_height = height;

    int mpgl_caps = MPGL_CAP_GL_LEGACY;
    if (!mpgl_create_window(p->mpglctx, mpgl_caps, d_width, d_height, flags))
        return -1;

    init_gl(vo, vo->dwidth, vo->dheight);

    return 0;
}

static void check_events(struct vo *vo)
{
    struct priv *p = vo->priv;
    int e = p->mpglctx->check_events(vo);
    if (e & VO_EVENT_RESIZE)
        resize(vo, vo->dwidth, vo->dheight);
}

static void prepare_texture(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct quad *q = p->quad;
    CVReturn error;

    CVOpenGLTextureRelease(p->texture);
    error = CVOpenGLTextureCacheCreateTextureFromImage(NULL,
                p->textureCache, p->pixelBuffer, 0, &p->texture);
    if(error != kCVReturnSuccess)
        mp_msg(MSGT_VO, MSGL_ERR,"[vo_corevideo] Failed to create OpenGL"
                                 " texture(%d)\n", error);

    CVOpenGLTextureGetCleanTexCoords(p->texture, q->lowerLeft, q->lowerRight,
                                                 q->upperRight, q->upperLeft);
}

static void do_render(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct quad *q = p->quad;
    GL *gl = p->mpglctx->gl;
    prepare_texture(vo);

    float x0 = 0;
    float y0 = 0;
    float  w = p->image_width;
    float  h = p->image_height;

    // vertically flips the image
    y0 += h;
    h = -h;

    float xm = x0 + w;
    float ym = y0 + h;

    gl->Enable(CVOpenGLTextureGetTarget(p->texture));
    gl->BindTexture(
            CVOpenGLTextureGetTarget(p->texture),
            CVOpenGLTextureGetName(p->texture));

    gl->Begin(GL_QUADS);
    gl->TexCoord2fv(q->lowerLeft);  gl->Vertex2f(x0, y0);
    gl->TexCoord2fv(q->upperLeft);  gl->Vertex2f(x0, ym);
    gl->TexCoord2fv(q->upperRight); gl->Vertex2f(xm, ym);
    gl->TexCoord2fv(q->lowerRight); gl->Vertex2f(xm, y0);
    gl->End();

    gl->Disable(CVOpenGLTextureGetTarget(p->texture));
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    p->mpglctx->swapGlBuffers(p->mpglctx);
    p->mpglctx->gl->Clear(GL_COLOR_BUFFER_BIT);
}

static uint32_t draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *p = vo->priv;
    CVReturn error;

    if (!p->textureCache || !p->pixelBuffer) {
        error = CVOpenGLTextureCacheCreate(NULL, 0, vo_cocoa_cgl_context(),
                    vo_cocoa_cgl_pixel_format(), 0, &p->textureCache);
        if(error != kCVReturnSuccess)
            mp_msg(MSGT_VO, MSGL_ERR,"[vo_corevideo] Failed to create OpenGL"
                                     " texture Cache(%d)\n", error);

        error = CVPixelBufferCreateWithBytes(NULL, mpi->width, mpi->height,
                    p->pixelFormat, mpi->planes[0], mpi->width * mpi->bpp / 8,
                    NULL, NULL, NULL, &p->pixelBuffer);
        if(error != kCVReturnSuccess)
            mp_msg(MSGT_VO, MSGL_ERR,"[vo_corevideo] Failed to create Pixel"
                                     "Buffer(%d)\n", error);
    }

    do_render(vo);
    return VO_TRUE;
}

static int query_format(struct vo *vo, uint32_t format)
{
    struct priv *p = vo->priv;
    const int flags = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW |
                      VFCAP_OSD | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN |
                      VOCAP_NOSLICES;
    switch (format) {
        case IMGFMT_YUY2:
            p->pixelFormat = kYUVSPixelFormat;
            return flags;

        case IMGFMT_RGB24:
            p->pixelFormat = k24RGBPixelFormat;
            return flags;

        case IMGFMT_ARGB:
            p->pixelFormat = k32ARGBPixelFormat;
            return flags;

        case IMGFMT_BGRA:
            p->pixelFormat = k32BGRAPixelFormat;
            return flags;
    }
    return 0;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;
    if (p->osd)
        mpgl_osd_destroy(p->osd);
    release_cv_entities(vo);
    mpgl_uninit(p->mpglctx);
}


static int preinit(struct vo *vo, const char *arg)
{
    struct priv *p = vo->priv;

    *p = (struct priv) {
        .mpglctx = mpgl_init(GLTYPE_COCOA, vo),
        .colorspace = MP_CSP_DETAILS_DEFAULTS,
        .quad = talloc_ptrtype(p, p->quad),
    };

    return 0;
}

static void draw_osd(struct vo *vo, struct osd_state *osd)
{
    struct priv *p = vo->priv;
    GL *gl = p->mpglctx->gl;
    assert(p->osd);

    gl->MatrixMode(GL_PROJECTION);
    gl->PushMatrix();
    gl->LoadIdentity();
    gl->Ortho(0, vo->dwidth, vo->dheight, 0, -1, 1);

    struct mp_osd_res res = {
        .w = vo->dwidth,
        .h = vo->dheight,
        .display_par = vo->monitor_par,
        .video_par = vo->aspdat.par,
    };

    if (aspect_scaling()) {
        res.ml = res.mr = p->ass_border_x;
        res.mt = res.mb = p->ass_border_y;
    }

    mpgl_osd_draw_legacy(p->osd, osd, res);

    gl->PopMatrix();
}

static CFStringRef get_cv_csp_matrix(struct vo *vo)
{
    struct priv *p = vo->priv;
    switch (p->colorspace.format) {
        case MP_CSP_BT_601:
            return kCVImageBufferYCbCrMatrix_ITU_R_601_4;
        case MP_CSP_BT_709:
            return kCVImageBufferYCbCrMatrix_ITU_R_709_2;
        case MP_CSP_SMPTE_240M:
            return kCVImageBufferYCbCrMatrix_SMPTE_240M_1995;
    }
    return kCVImageBufferYCbCrMatrix_ITU_R_601_4;
}

static void set_yuv_colorspace(struct vo *vo)
{
    struct priv *p = vo->priv;
    CVBufferSetAttachment(p->pixelBuffer,
                          kCVImageBufferYCbCrMatrixKey, get_cv_csp_matrix(vo),
                          kCVAttachmentMode_ShouldPropagate);
    vo->want_redraw = true;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;
    switch (request) {
        case VOCTRL_DRAW_IMAGE:
            return draw_image(vo, data);
        case VOCTRL_QUERY_FORMAT:
            return query_format(vo, *(uint32_t*)data);
        case VOCTRL_ONTOP:
            p->mpglctx->ontop(vo);
            return VO_TRUE;
        case VOCTRL_FULLSCREEN:
            p->mpglctx->fullscreen(vo);
            resize(vo, vo->dwidth, vo->dheight);
            return VO_TRUE;
        case VOCTRL_GET_PANSCAN:
            return VO_TRUE;
        case VOCTRL_SET_PANSCAN:
            resize(vo, vo->dwidth, vo->dheight);
            return VO_TRUE;
        case VOCTRL_UPDATE_SCREENINFO:
            p->mpglctx->update_xinerama_info(vo);
            return VO_TRUE;
        case VOCTRL_REDRAW_FRAME:
            do_render(vo);
            return VO_TRUE;
        case VOCTRL_SET_YUV_COLORSPACE:
            p->colorspace.format = ((struct mp_csp_details *)data)->format;
            set_yuv_colorspace(vo);
            return VO_TRUE;
        case VOCTRL_GET_YUV_COLORSPACE:
            *(struct mp_csp_details *)data = p->colorspace;
            return VO_TRUE;
    }
    return VO_NOTIMPL;
}

const struct vo_driver video_out_corevideo = {
    .is_new = true,
    .info = &(const vo_info_t) {
        "Mac OS X Core Video",
        "corevideo",
        "Nicolas Plourde <nicolas.plourde@gmail.com> and others",
        ""
    },
    .preinit = preinit,
    .config = config,
    .control = control,
    .draw_osd = draw_osd,
    .flip_page = flip_page,
    .check_events = check_events,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
};
