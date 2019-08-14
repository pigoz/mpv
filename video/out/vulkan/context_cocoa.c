/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "video/out/gpu/context.h"
#include "video/out/cocoa_common.h"

#include "common.h"
#include "context.h"
#include "utils.h"

struct priv {
    struct mpvk_ctx vk;
};

static void cocoa_vk_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    ra_vk_ctx_uninit(ctx);
    mpvk_uninit(&p->vk);
    vo_cocoa_uninit(ctx->vo);
}

static bool cocoa_vk_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    struct mpvk_ctx *vk = &p->vk;
    int msgl = ctx->opts.probing ? MSGL_V : MSGL_ERR;

    if (!mpvk_init(vk, ctx, VK_MVK_MACOS_SURFACE_EXTENSION_NAME))
        goto error;

    vo_cocoa_init(ctx->vo);
    vo_cocoa_config_window(ctx->vo);

    MP_ERR(ctx, "%p\n", vo_cocoa_video_view(ctx->vo));

    VkMacOSSurfaceCreateInfoMVK info = {
        .sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK,
        .pView = vo_cocoa_video_view(ctx->vo),
        .pNext = NULL,
        .flags = 0,
    };

    VkInstance inst = vk->vkinst->instance;
    VkResult res = vkCreateMacOSSurfaceMVK(inst, &info, NULL, &vk->surface);
    if (res != VK_SUCCESS) {
        MP_MSG(ctx, msgl, "Failed creating cocoa surface\n");
        goto error;
    }

    /* Because in cocoa clients render whenever they receive a callback from
     * the compositor, and the fact that the compositor usually stops sending
     * callbacks once the surface is no longer visible, using FIFO here would
     * mean the entire player would block on acquiring swapchain images. Hence,
     * use MAILBOX to guarantee that there'll always be a swapchain image and
     * the player won't block waiting on those */
    if (!ra_vk_ctx_init(ctx, vk, VK_PRESENT_MODE_FIFO_KHR))
        goto error;

    // ra_add_native_resource(ctx->ra, "cocoa", ctx->vo->cocoa->display);

    return true;

error:
    cocoa_vk_uninit(ctx);
    return false;
}

static bool resize(struct ra_ctx *ctx)
{
    return ra_vk_ctx_resize(ctx, ctx->vo->dwidth, ctx->vo->dheight);
}

static bool cocoa_vk_reconfig(struct ra_ctx *ctx)
{
    if (!vo_cocoa_config_window(ctx->vo))
        return false;

    return true;
}

static int cocoa_vk_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    int ret = vo_cocoa_control(ctx->vo, events, request, arg);
    if (*events & VO_EVENT_RESIZE) {
        if (!resize(ctx))
            return VO_ERROR;
    }
    return ret;
}

// static void cocoa_vk_wakeup(struct ra_ctx *ctx)
// {
//     vo_cocoa_wakeup(ctx->vo);
// }
// 
// static void cocoa_vk_wait_events(struct ra_ctx *ctx, int64_t until_time_us)
// {
//     vo_cocoa_wait_events(ctx->vo, until_time_us);
// }

const struct ra_ctx_fns ra_ctx_vulkan_cocoa = {
    .type           = "vulkan",
    .name           = "cocoavk",
    .reconfig       = cocoa_vk_reconfig,
    .control        = cocoa_vk_control,
    // .wakeup         = cocoa_vk_wakeup,
    // .wait_events    = cocoa_vk_wait_events,
    .init           = cocoa_vk_init,
    .uninit         = cocoa_vk_uninit,
};
