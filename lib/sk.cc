#include <cstddef>
#include <core/SkImage.h>

#define  SK_VULKAN
#include <gpu/vk/GrVkBackendContext.h>
#include <gpu/ganesh/vk/GrVkBackendSurface.h>
#include <gpu/GrBackendSurface.h>
#include <gpu/GrDirectContext.h>
#include <gpu/vk/VulkanExtensions.h>
#include <core/SkPath.h>
#include <core/SkFont.h>
#include <core/SkRRect.h>
#include <core/SkBitmap.h>
#include <core/SkCanvas.h>
#include <core/SkColorSpace.h>
#include <core/SkSurface.h>
#include <core/SkFontMgr.h>
#include <core/SkFontMetrics.h>
#include <core/SkPathMeasure.h>
#include <core/SkPathUtils.h>
#include <utils/SkParsePath.h>
#include <core/SkTextBlob.h>
#include <effects/SkGradientShader.h>
#include <effects/SkImageFilters.h>
#include <effects/SkDashPathEffect.h>
#include <core/SkStream.h>
#include <modules/svg/include/SkSVGDOM.h>
#include <modules/svg/include/SkSVGNode.h>
#include <core/SkAlphaType.h>
#include <core/SkColor.h>
#include <core/SkColorType.h>
#include <core/SkImageInfo.h>
#include <core/SkRefCnt.h>
#include <core/SkTypes.h>
#include <gpu/GrDirectContext.h>
#include <gpu/ganesh/vk/GrVkDirectContext.h>
#include <gpu/ganesh/SkSurfaceGanesh.h>
#include <gpu/vk/GrVkBackendContext.h>
#include <gpu/vk/VulkanExtensions.h>
//#include <assert.h>

extern "C" {
#include <import>
}

#undef get
#undef clear
#undef fill
#undef move
#undef submit

//#include <skia/tools/gpu/vk/VkTestUtils.h>

struct Skia {
    sk_sp<GrDirectContext> ctx;
};

extern "C" {

/// initialize skia from vulkan-resources
skia_t skia_init_vk(handle_t vk_instance, handle_t phys, handle_t device, handle_t queue, unsigned int graphics_family, unsigned int vk_version) {
    //GrBackendFormat gr_conv = GrBackendFormat::MakeVk(VK_FORMAT_R8G8B8_SRGB);
    GrVkBackendContext grc {
        (VkInstance)vk_instance,
        (VkPhysicalDevice)phys,
        (VkDevice)device,
        (VkQueue)queue,
        graphics_family,
        vk_version
    };
    grc.fMaxAPIVersion = vk_version;
    //grc.fVkExtensions = new GrVkExtensions(); // internal needs population perhaps
    grc.fGetProc = [](const char *name, VkInstance inst, VkDevice dev) -> PFN_vkVoidFunction {
        return !dev ? vkGetInstanceProcAddr(inst, name) : vkGetDeviceProcAddr(dev, name);
    };

    Skia* sk = new Skia();
    
    sk_sp<GrDirectContext> ctx = GrDirectContexts::MakeVulkan(grc);
    GrDirectContext* ctx1 = ctx.get();

    sk->ctx = GrDirectContexts::MakeVulkan(grc);
    
    assert(sk->ctx, "could not obtain GrVulkanContext");
    return sk;
}

extern "C" { SkColor sk_color(object any); }

extern "C" { path path_with_cstr(path a, cstr cs); }

none sk_init(sk a);
none sk_dealloc(sk a);

none sk_resize(sk a, i32 w, i32 h) {
    a->width  = w;
    a->height = h;
    resize(a->tx, w, h);
    sk_dealloc(a);
    sk_init(a);
}

none sk_init(sk a) {
    trinity t = a->t;
    a->skia = (Skia*)skia_init_vk(
        t->instance,
        t->physical_device,
        t->device,
        t->queue,
        t->queue_family_index, VK_API_VERSION_1_2);

    verify(a->width > 0 && a->height > 0, "sk requires width and height");
    if (!a->tx) {
        texture tx = texture(t, a->t, width, a->width, height, a->height, 
            surface, Surface_color,
            format, Pixel_rgba8,
            linear, true, mip_levels, 1, layer_count, 1);
        a->tx = (texture)A_hold((object)tx);
    }
    
    GrVkImageInfo info       = {};
    info.fImage              = (VkImage)a->tx->vk_image;
    //info.fImageLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.fImageTiling        = VK_IMAGE_TILING_OPTIMAL;
    info.fFormat             = VK_FORMAT_R8G8B8A8_UNORM;
    info.fLevelCount         = 1;
    info.fCurrentQueueFamily = VK_QUEUE_FAMILY_IGNORED;
    
    GrBackendTexture backend_texture = GrBackendTextures::MakeVk(
        a->width, a->height, info);

    GrDirectContext* direct_ctx = a->skia->ctx.get();
    sk_sp<SkSurface> surface = SkSurfaces::WrapBackendTexture(
        direct_ctx, backend_texture, kTopLeft_GrSurfaceOrigin, 1,
        kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(),
        (const SkSurfaceProps *)null, (SkSurfaces::TextureReleaseProc)null);
    
    SkSafeRef(surface.get());
    a->sk_surface = (ARef)surface.get();
    a->sk_canvas  = (ARef)((SkSurface*)a->sk_surface)->getCanvas();
    ((SkCanvas*)a->sk_canvas)->clear(SK_ColorWHITE); // <--- clear to black here
    a->sk_path    = (ARef)new SkPath();
    a->state      = array(alloc, 16);
    save(a);
}

none sk_dealloc(sk a) {
    SkSafeUnref((SkSurface*)a->sk_surface);
}

none sk_move_to(sk a, f32 x, f32 y) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ((SkPath*)a->sk_path)->moveTo(x, y);
}

none sk_line_to(sk a, f32 x, f32 y) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ((SkPath*)a->sk_path)->lineTo(x, y);
}

none sk_rect_to(sk a, f32 x, f32 y, f32 w, f32 h) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ((SkPath*)a->sk_path)->addRect(SkRect::MakeXYWH(x, y, w, h));
}

none sk_rounded_rect_to(sk a, f32 x, f32 y, f32 w, f32 h, f32 sx, f32 sy) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ((SkPath*)a->sk_path)->addRoundRect(SkRect::MakeXYWH(x, y, w, h), sx, sy);
}

none sk_arc_to(sk a, f32 x1, f32 y1, f32 x2, f32 y2, f32 radius) {
    SkCanvas*  sk     = (SkCanvas*)a->sk_canvas;
    draw_state ds     = (draw_state)last(a->state);
    ((SkPath*)a->sk_path)->arcTo(x1, y1, x2, y2, radius);
}

none sk_arc(sk a, f32 center_x, f32 center_y, f32 radius, f32 start_angle, f32 end_angle) {
    SkCanvas*  sk     = (SkCanvas*)a->sk_canvas;
    draw_state ds     = (draw_state)last(a->state);
    SkRect     rect   = SkRect::MakeLTRB(
        center_x - radius, center_y - radius,
        center_x + radius, center_y + radius);
    f32 start_deg = start_angle * 180.0 / M_PI;
    f32 sweep_deg = (end_angle - start_angle) * 180.0 / M_PI;
    ((SkPath*)a->sk_path)->addArc(rect, start_deg, sweep_deg);
}

none sk_draw_fill_preserve(sk a) {
    SkCanvas*  sk     = (SkCanvas*)a->sk_canvas;
    draw_state ds     = (draw_state)last(a->state);
    SkPaint    paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(ds->fill_color); // assuming this exists in your draw_state
    paint.setAntiAlias(true); 
    sk->drawPath(*(SkPath*)a->sk_path, paint);
}

none sk_draw_fill(sk a) {
    sk_draw_fill_preserve(a);
    ((SkPath*)a->sk_path)->reset();
}

none sk_draw_stroke_preserve(sk a) {
    SkCanvas*  sk     = (SkCanvas*)a->sk_canvas;
    draw_state ds     = (draw_state)last(a->state);
    if (!ds->stroke) return;
    SkPaint    paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(ds->stroke->width);
    paint.setColor(ds->stroke_color); // assuming this exists in your draw_state
    paint.setAntiAlias(true); 
    sk->drawPath(*(SkPath*)a->sk_path, paint);
}

none sk_draw_stroke(sk a) {
    sk_draw_stroke_preserve(a);
    ((SkPath*)a->sk_path)->reset();
}

none sk_cubic(sk a, f32 cp1_x, f32 cp1_y, f32 cp2_x, f32 cp2_y, f32 ep_x, f32 ep_y) {
    SkCanvas*  sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ((SkPath*)a->sk_path)->cubicTo(cp1_x, cp1_y, cp2_x, cp2_y, ep_x, ep_y);
}

none sk_quadratic(sk a, f32 cp_x, f32 cp_y, f32 ep_x, f32 ep_y) {
    SkCanvas*  sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ((SkPath*)a->sk_path)->quadTo(cp_x, cp_y, ep_x, ep_y);
}

none sk_save(sk a) {
    draw_state ds;
    if (len(a->state)) {
        draw_state prev = (draw_state)last(a->state);
        ds = (draw_state)copy(prev);
    } else {
        ds = draw_state();
        set_default(ds);
    }
    push(a->state, (object)ds);
}

none sk_set_font(sk a, font f) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    if (ds->font != f) {
        drop((object)ds->font);
        ds->font = (font)hold((object)f);
    }
}

none sk_set_stroke(sk a, stroke s) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    if (ds->stroke != s) {
        drop((object)ds->stroke);
        ds->stroke = (stroke)hold((object)s);
    }
}

none sk_restore(sk a) {
    if (!len(a->state))
        return;
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    drop((object)ds->stroke);
    drop((object)ds->font);
    pop(a->state);
}

none sk_fill_color(sk a, object clr) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ds->fill_color = sk_color(clr);
}

none sk_stroke_color(sk a, object clr) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ds->stroke_color = sk_color(clr);
}

none sk_clear(sk a, object clr) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    SkColor fill = clr ? sk_color(clr) : SK_ColorWHITE;
    sk->clear(fill);
}

void transition_image_layout(trinity, VkImage, VkImageLayout, VkImageLayout, int, int, int, int, bool);

none sk_prepare(sk a) {
    
}

none sk_sync(sk a) {
    GrDirectContext* direct_ctx = a->skia->ctx.get();
    //transition(a->tx, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    direct_ctx->flush();
    if (!a->once) {
        a->once = true;
        transition(a->tx, (i32)VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    } else {
        transition(a->tx, (i32)VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
    direct_ctx->submit();
}

none sk_output_mode(sk a, bool output) {
    VkImageLayout to = output ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : 
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkImageLayout fr = output ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : 
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    //transition(a->tx, to);
    transition_image_layout(a->t, (VkImage)a->tx->vk_image, fr, to,
        0, 1, 0, 1, false);
    a->tx->vk_layout = to;
}

define_mod(sk, canvas)

}

/*

after RT render, port all of canvas.hpp and this code in trinity

struct state {
    image       img;
    f32         outline_sz = 0.0;
    f32         font_scale = 1.0;
    f32         opacity    = 1.0;
    mat4f       m;
    i32         color;
    vec4f       clip;
    vec2f       blur;
    ion::font   font;
    SkPaint     ps;
    mat4f       model, view, proj;
};

/// canvas renders to image, and can manage the renderer/resizing
struct ICanvas {
    GrDirectContext     *ctx = null;
    VkEngine               e = null;
    VkhPresenter    renderer = null;
    sk_sp<SkSurface> sk_surf = null;
    SkCanvas      *sk_canvas = null;
    vec2i                 sz = { 0, 0 };
    VkhImage        vk_image = null;
    vec2d          dpi_scale = { 1, 1 };

    struct state {
        ion::image  img;
        double      outline_sz = 0.0;
        double      font_scale = 1.0;
        double      opacity    = 1.0;
        m44d        m;
        rgbad       color;
        graphics::shape clip;
        vec2d       blur;
        ion::font   font;
        SkPaint     ps;
        glm::mat4   model, view, proj;
    };

    state *top = null;
    doubly<state> stack;

    void outline_sz(double sz) {
        top->outline_sz = sz;
    }

    void color(rgbad &c) {
        top->color = c;
    }

    void opacity(double o) {
        top->opacity = o;
    }

    /// can be given an image
    void sk_resize(VkhImage &image, int width, int height) {
        if (vk_image)
            vkh_image_drop(vk_image);
        if (!image) {
            vk_image = vkh_image_create(
                e->vkh, VK_FORMAT_R8G8B8A8_UNORM, u32(width), u32(height),
                VK_IMAGE_TILING_OPTIMAL, VKH_MEMORY_USAGE_GPU_ONLY,
                VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);

            // test code to clear the image with a color (no result from SkCanvas currently!)
            // this results in a color drawn after the renderer blits the VkImage to the window
            // this image is given to 
            VkDevice device = e->vk_device->device;
            VkQueue  queue  = e->renderer->queue;
            VkImage  image  = vk_image->image;

            VkCommandBuffer commandBuffer = e->vk_device->command_begin();

            // Assume you have a VkDevice, VkPhysicalDevice, VkQueue, and VkCommandBuffer already set up.
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );

            // Clear the image with blue color
            VkClearColorValue clearColor = { 0.4f, 0.0f, 0.5f, 1.0f };
            vkCmdClearColorImage(
                commandBuffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                &clearColor,
                1,
                &barrier.subresourceRange
            );

            // Transition image layout to SHADER_READ_ONLY_OPTIMAL
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );

            e->vk_device->command_submit(commandBuffer);

        } else {
            /// option: we can know dpi here as declared by the user
            vk_image = vkh_image_grab(image);
            assert(width == vk_image->width && height == vk_image->height);
        }
        
        sz = vec2i { width, height };
        ///
        ctx                     = Skia::Context(e)->sk_context;
        auto imi                = GrVkImageInfo { };
        imi.fImage              = vk_image->image;
        imi.fImageTiling        = VK_IMAGE_TILING_OPTIMAL;
        imi.fImageLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        imi.fFormat             = VK_FORMAT_R8G8B8A8_UNORM;
        imi.fImageUsageFlags    = VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;//VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // i dont think so.
        imi.fSampleCount        = 1;
        imi.fLevelCount         = 1;
        imi.fCurrentQueueFamily = e->vk_gpu->indices.graphicsFamily.value();
        imi.fProtected          = GrProtected::kNo;
        imi.fSharingMode        = VK_SHARING_MODE_EXCLUSIVE;

        auto color_space = SkColorSpace::MakeSRGB();
        auto rt = GrBackendRenderTarget { sz.x, sz.y, imi };
        sk_surf = SkSurfaces::WrapBackendRenderTarget(ctx, rt,
                    kTopLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType,
                    color_space, null);
        sk_canvas = sk_surf->getCanvas();
        dpi_scale = e->vk_gpu->dpi_scale;
        identity();
    }

    void app_resize() {
        /// these should be updated (VkEngine can have VkWindow of sort eventually if we want multiple)
        float sx, sy;
        u32 width, height;
        vkh_presenter_get_size(renderer, &width, &height, &sx, &sy); /// vkh/vk should have both vk engine and glfw facility
        VkhImage img = null;
        sk_resize(img, width, height);
        vkh_presenter_build_blit_cmd(renderer, vk_image->image,
            width / dpi_scale.x, height / dpi_scale.y);
    }

    SkPath *sk_path(graphics::shape &sh) {
        graphics::shape::sdata &shape = sh.ref<graphics::shape::sdata>();
        // shape.sk_path 
        if (!shape.sk_path) {
            shape.sk_path = new SkPath { };
            SkPath &p     = *(SkPath*)shape.sk_path;

            /// efficient serialization of types as Skia does not spend the time to check for these primitives
            if (shape.type == typeof(rectd)) {
                rectd &m = sh->bounds;
                SkRect r = SkRect {
                    float(m.x), float(m.y), float(m.x + m.w), float(m.y + m.h)
                };
                p.Rect(r);
            } else if (shape.type == typeof(Rounded<double>)) {
                Rounded<double>::rdata &m = sh->bounds.mem->ref<Rounded<double>::rdata>();
                p.moveTo  (m.tl_x.x, m.tl_x.y);
                p.lineTo  (m.tr_x.x, m.tr_x.y);
                p.cubicTo (m.c0.x,   m.c0.y, m.c1.x, m.c1.y, m.tr_y.x, m.tr_y.y);
                p.lineTo  (m.br_y.x, m.br_y.y);
                p.cubicTo (m.c0b.x,  m.c0b.y, m.c1b.x, m.c1b.y, m.br_x.x, m.br_x.y);
                p.lineTo  (m.bl_x.x, m.bl_x.y);
                p.cubicTo (m.c0c.x,  m.c0c.y, m.c1c.x, m.c1c.y, m.bl_y.x, m.bl_y.y);
                p.lineTo  (m.tl_y.x, m.tl_y.y);
                p.cubicTo (m.c0d.x,  m.c0d.y, m.c1d.x, m.c1d.y, m.tl_x.x, m.tl_x.y);
            } else {
                graphics::shape::sdata &m = *sh.data;
                for (mx &o:m.ops) {
                    type_t t = o.type();
                    if (t == typeof(Movement)) {
                        Movement m(o);
                        p.moveTo(m->x, m->y);
                    } else if (t == typeof(Line)) {
                        Line l(o);
                        p.lineTo(l->origin.x, l->origin.y); /// todo: origin and to are swapped, i believe
                    }
                }
                p.close();
            }
        }

        /// issue here is reading the data, which may not be 'sdata', but Rect, Rounded
        /// so the case below is 
        if (bool(shape.sk_offset) && shape.cache_offset == shape.offset)
            return (SkPath*)shape.sk_offset;
        ///
        if (!std::isnan(shape.offset) && shape.offset != 0) {
            assert(shape.sk_path); /// must have an actual series of shape operations in skia
            ///
            delete (SkPath*)shape.sk_offset;

            SkPath *o = (SkPath*)shape.sk_offset;
            shape.cache_offset = shape.offset;
            ///
            SkPath  fpath;
            SkPaint cp = SkPaint(top->ps);
            cp.setStyle(SkPaint::kStroke_Style);
            cp.setStrokeWidth(std::abs(shape.offset) * 2);
            cp.setStrokeJoin(SkPaint::kRound_Join);

            SkPathStroker2 stroker;
            SkPath offset_path = stroker.getFillPath(*(SkPath*)shape.sk_path, cp);
            shape.sk_offset = new SkPath(offset_path);
            
            auto vrbs = ((SkPath*)shape.sk_path)->countVerbs();
            auto pnts = ((SkPath*)shape.sk_path)->countPoints();
            std::cout << "sk_path = " << (void *)shape.sk_path << ", pointer = " << (void *)this << " verbs = " << vrbs << ", points = " << pnts << "\n";
            ///
            if (shape.offset < 0) {
                o->reverseAddPath(fpath);
                o->setFillType(SkPathFillType::kWinding);
            } else
                o->addPath(fpath);
            ///
            return o;
        }
        return (SkPath*)shape.sk_path;
    }

    void font(ion::font &f) { 
        top->font = f;
    }
    
    void save() {
        state &s = stack->push();
        if (top) {
            s = *top;
        } else {
            s.ps = SkPaint { };
        }
        sk_canvas->save();
        top = &s;
    }

    void identity() {
        sk_canvas->resetMatrix();
        sk_canvas->scale(dpi_scale.x, dpi_scale.y);
    }

    void set_matrix() {
    }

    m44d get_matrix() {
        SkM44 skm = sk_canvas->getLocalToDevice();
        m44d res(0.0);
        return res;
    }


    void    clear()        { sk_canvas->clear(sk_color(top->color)); }
    void    clear(rgbad c) { sk_canvas->clear(sk_color(c)); }

    void    flush() {
        ctx->flush();
        ctx->submit();
    }

    void  restore() {
        stack->pop();
        top = stack->len() ? &stack->last() : null;
        sk_canvas->restore();
    }

    vec2i size() { return sz; }

    /// console would just think of everything in char units. like it is.
    /// measuring text would just be its length, line height 1.
    text_metrics measure(str &text) {
        SkFontMetrics mx;
        SkFont     &font = font_handle(top->font);
        auto         adv = font.measureText(text.cs(), text.len(), SkTextEncoding::kUTF8);
        auto          lh = font.getMetrics(&mx);

        return text_metrics {
            adv,
            abs(mx.fAscent) + abs(mx.fDescent),
            mx.fAscent,
            mx.fDescent,
            lh,
            mx.fCapHeight
        };
    }

    double measure_advance(char *text, size_t len) {
        SkFont     &font = font_handle(top->font);
        auto         adv = font.measureText(text, len, SkTextEncoding::kUTF8);
        return (double)adv;
    }

    /// the text out has a rect, controls line height, scrolling offset and all of that nonsense we need to handle
    /// as a generic its good to have the rect and alignment enums given.  there simply isnt a user that doesnt benefit
    /// it effectively knocks out several redundancies to allow some components to be consolidated with style difference alone
    str ellipsis(str &text, rectd &rect, text_metrics &tm) {
        const str el = "...";
        str       cur, *p = &text;
        int       trim = p->len();
        tm             = measure((str &)el);
        
        if (tm.w >= rect.w)
            trim = 0;
        else
            for (;;) {
                tm = measure(*p);
                if (tm.w <= rect.w || trim == 0)
                    break;
                if (tm.w > rect.w && trim >= 1) {
                    cur = text.mid(0, --trim) + el;
                    p   = &cur;
                }
            }
        return (trim == 0) ? "" : (p == &text) ? text : cur;
    }


    void image(ion::SVG &image, rectd &rect, alignment &align, vec2d &offset) {
        SkPaint ps = SkPaint(top->ps);
        vec2d  pos = { 0, 0 };
        vec2i  isz = image.sz();
        
        ps.setColor(sk_color(top->color));
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        /// now its just of matter of scaling the little guy to fit in the box.
        real scx = rect.w / isz.x;
        real scy = rect.h / isz.y;
        real sc  = (scy > scx) ? scx : scy;
        
        /// no enums were harmed during the making of this function (again)
        pos.x = mix(rect.x, rect.x + rect.w - isz.x * sc, align.x);
        pos.y = mix(rect.y, rect.y + rect.h - isz.y * sc, align.y);
        
        sk_canvas->save();
        sk_canvas->translate(pos.x + offset.x, pos.y + offset.y);
        
        sk_canvas->scale(sc, sc);
        image.render(sk_canvas, rect.w, rect.h);
        sk_canvas->restore();
    }

    void image(ion::image &image, rectd &rect, alignment &align, vec2d &offset, bool attach_tx) {
        SkPaint ps = SkPaint(top->ps);
        vec2d  pos = { 0, 0 };
        vec2i  isz = image.sz();
        
        ps.setColor(sk_color(top->color));
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        /// cache SkImage using memory attachments
        attachment *att = image.mem->find_attachment("sk-image");
        sk_sp<SkImage> *im;
        if (!att) {
            SkBitmap bm;
            rgba8          *px = image.pixels();
            //memset(px, 255, 640 * 360 * 4);
            SkImageInfo   info = SkImageInfo::Make(isz.x, isz.y, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
            sz_t        stride = image.stride() * sizeof(rgba8);
            bm.installPixels(info, px, stride);
            sk_sp<SkImage> bm_image = bm.asImage();
            im = new sk_sp<SkImage>(bm_image);
            if (attach_tx)
                att = image.mem->attach("sk-image", im, [im]() { delete im; });
        }
        
        /// now its just of matter of scaling the little guy to fit in the box.
        real scx = rect.w / isz.x;
        real scy = rect.h / isz.y;
        
        if (!align.is_default) {
            scx   = scy = (scy > scx) ? scx : scy;
            /// no enums were harmed during the making of this function
            pos.x = mix(rect.x, rect.x + rect.w - isz.x * scx, align.x);
            pos.y = mix(rect.y, rect.y + rect.h - isz.y * scy, align.y);
            
        } else {
            /// if alignment is default state, scale directly by bounds w/h, position at bounds x/y
            pos.x = rect.x;
            pos.y = rect.y;
        }
        
        sk_canvas->save();
        sk_canvas->translate(pos.x + offset.x, pos.y + offset.y);
        
        SkCubicResampler cubicResampler { 1.0f/3, 1.0f/3 };
        SkSamplingOptions samplingOptions(cubicResampler);

        sk_canvas->scale(scx, scy);
        SkImage *img = im->get();
        sk_canvas->drawImage(img, SkScalar(0), SkScalar(0), samplingOptions);
        sk_canvas->restore();

        if (!attach_tx) {
            delete im;
            ctx->flush();
            ctx->submit();
        }
    }

    /// the lines are most definitely just text() calls, it should be up to the user to perform multiline.
    void text(str &text, rectd &rect, alignment &align, vec2d &offset, bool ellip, rectd *placement) {
        SkPaint ps = SkPaint(top->ps);
        ps.setColor(sk_color(top->color));
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        SkFont  &f = font_handle(top->font);
        vec2d  pos = { 0, 0 };
        str  stext;
        str *ptext = &text;
        text_metrics tm;
        if (ellip) {
            stext  = ellipsis(text, rect, tm);
            ptext  = &stext;
        } else
            tm     = measure(*ptext);
        auto    tb = SkTextBlob::MakeFromText(ptext->cs(), ptext->len(), (const SkFont &)f, SkTextEncoding::kUTF8);
        pos.x = mix(rect.x, rect.x + rect.w - tm.w, align.x);
        pos.y = mix(rect.y, rect.y + rect.h - tm.h, align.y);
        double skia_y_offset = (tm.descent + -tm.ascent) / 1.5;
        /// set placement rect if given (last paint is what was rendered)
        if (placement) {
            placement->x = pos.x + offset.x;
            placement->y = pos.y + offset.y;
            placement->w = tm.w;
            placement->h = tm.h;
        }
        sk_canvas->drawTextBlob(
            tb, SkScalar(pos.x + offset.x),
                SkScalar(pos.y + offset.y + skia_y_offset), ps);
    }

    void clip(rectd &rect) {
        SkRect   r = SkRect {
            SkScalar(rect.x),          SkScalar(rect.y),
            SkScalar(rect.x + rect.w), SkScalar(rect.y + rect.h) };
        sk_canvas->clipRect(r);
    }

    void outline(array<glm::vec2> &line, bool is_fill = false) {
        SkPaint ps = SkPaint(top->ps);
        ps.setAntiAlias(true);
        ps.setColor(sk_color(top->color));
        ps.setStrokeWidth(is_fill ? 0 : top->outline_sz);
        ps.setStroke(!is_fill);
        glm::vec2 *a = null;
        SkPath path;
        for (glm::vec2 &b: line) {
            SkPoint bp = { b.x, b.y };
            if (a) {
                path.lineTo(bp);
            } else {
                path.moveTo(bp);
            }
            a = &b;
        }
        sk_canvas->drawPath(path, ps);
    }

    void projection(glm::mat4 &m, glm::mat4 &v, glm::mat4 &p) {
        top->model      = m;
        top->view       = v;
        top->proj       = p;
    }

    void outline(array<glm::vec3> &v3, bool is_fill = false) {
        glm::vec2 sz = { this->sz.x / this->dpi_scale.x, this->sz.y / this->dpi_scale.y };
        array<glm::vec2> projected { v3.len() };
        for (glm::vec3 &vertex: v3) {
            glm::vec4 cs  = top->proj * top->view * top->model * glm::vec4(vertex, 1.0f);
            glm::vec3 ndc = cs / cs.w;
            float screenX = ((ndc.x + 1) / 2.0) * sz.x;
            float screenY = ((1 - ndc.y) / 2.0) * sz.y;
            glm::vec2  v2 = { screenX, screenY };
            projected    += v2;
        }
        outline(projected, is_fill);
    }

    void line(glm::vec3 &a, glm::vec3 &b) {
        array<glm::vec3> ab { size_t(2) };
        ab.push(a);
        ab.push(b);
        outline(ab);
    }

    void arc(glm::vec3 position, real radius, real startAngle, real endAngle, bool is_fill = false) {
        const int segments = 36;
        ion::array<glm::vec3> arcPoints { size_t(segments) };
        float angleStep = (endAngle - startAngle) / segments;

        for (int i = 0; i <= segments; ++i) {
            float     angle = glm::radians(startAngle + angleStep * i);
            glm::vec3 point;
            point.x = position.x + radius * cos(angle);
            point.y = position.y;
            point.z = position.z + radius * sin(angle);
            glm::vec4 viewSpacePoint = top->view * glm::vec4(point, 1.0f);
            glm::vec3 clippingSp     = glm::vec3(viewSpacePoint);
            arcPoints += clippingSp;
        }
        arcPoints.set_size(segments);
        outline(arcPoints, is_fill);
    }

    void outline(rectd &rect) {
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setAntiAlias(true);
        ps.setColor(sk_color(top->color));
        ps.setStrokeWidth(top->outline_sz);
        ps.setStroke(true);
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        draw_rect(rect, ps);
    }

    void outline(graphics::shape &shape) {
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setAntiAlias(!shape.is_rect());
        ps.setColor(sk_color(top->color));
        ps.setStrokeWidth(top->outline_sz);
        ps.setStroke(true);
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        sk_canvas->drawPath(*sk_path(shape), ps);
    }

    void cap(graphics::cap &c) {
        top->ps.setStrokeCap(c == graphics::cap::blunt ? SkPaint::kSquare_Cap :
                             c == graphics::cap::round ? SkPaint::kRound_Cap  :
                                                         SkPaint::kButt_Cap);
    }

    void join(graphics::join &j) {
        top->ps.setStrokeJoin(j == graphics::join::bevel ? SkPaint::kBevel_Join :
                              j == graphics::join::round ? SkPaint::kRound_Join  :
                                                          SkPaint::kMiter_Join);
    }

    void translate(vec2d &tr) {
        sk_canvas->translate(SkScalar(tr.x), SkScalar(tr.y));
    }

    void scale(vec2d &sc) {
        sk_canvas->scale(SkScalar(sc.x), SkScalar(sc.y));
    }

    void rotate(double degs) {
        sk_canvas->rotate(degs);
    }

    void draw_rect(rectd &rect, SkPaint &ps) {
        SkRect   r = SkRect {
            SkScalar(rect.x),          SkScalar(rect.y),
            SkScalar(rect.x + rect.w), SkScalar(rect.y + rect.h) };
        
        if (rect.rounded) {
            SkRRect rr;
            SkVector corners[4] = {
                { float(rect.r_tl.x), float(rect.r_tl.y) },
                { float(rect.r_tr.x), float(rect.r_tr.y) },
                { float(rect.r_br.x), float(rect.r_br.y) },
                { float(rect.r_bl.x), float(rect.r_bl.y) }
            };
            rr.setRectRadii(r, corners);
            sk_canvas->drawRRect(rr, ps);
        } else {
            sk_canvas->drawRect(r, ps);
        }
    }

    void fill(rectd &rect) {
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setColor(sk_color(top->color));
        ps.setAntiAlias(true);
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));

        draw_rect(rect, ps);
    }

    // we are to put everything in path.
    void fill(graphics::shape &path) {
        if (path.is_rect())
            return fill(path->bounds);
        ///
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setAntiAlias(!path.is_rect());
        ps.setColor(sk_color(top->color));
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        sk_canvas->drawPath(*sk_path(path), ps);
    }

    void clip(graphics::shape &path) {
        sk_canvas->clipPath(*sk_path(path));
    }

    void gaussian(float *sz, rectd &crop) {
        SkImageFilters::CropRect crect = { };
        if (crop) {
            SkRect rect = { SkScalar(crop.x),          SkScalar(crop.y),
                            SkScalar(crop.x + crop.w), SkScalar(crop.y + crop.h) };
            crect       = SkImageFilters::CropRect(rect);
        }
        sk_sp<SkImageFilter> filter = SkImageFilters::Blur(sz[0], sz[1], nullptr, crect);
        memcpy(top->blur, sz, sizeof(float) * 2);
        top->ps.setImageFilter(std::move(filter));
    }

    SkFont *font_handle(const char* font) {
        /// dpi scaling is supported at the SkTypeface level, just add the scale x/y
        auto t = SkTypeface::MakeFromFile(font);
        font->sk_font = new SkFont(t);
        ((SkFont*)font->sk_font)->setSize(font->sz);
        return *(SkFont*)font->sk_font;
    }
    type_register(ICanvas);
};

mx_implement(Canvas, mx);

// we need to give VkEngine too, if we want to support image null
Canvas::Canvas(VkhImage image) : Canvas() {
    data->e = image->vkh->e;
    data->sk_resize(image, image->width, image->height);
    data->save();
}

*/