#include <cstddef>
#include <skia/include/core/SkImage.h>

#define  SK_VULKAN
#include <skia/gpu/vk/GrVkBackendContext.h>
#include <skia/gpu/GrBackendSurface.h>
#include <skia/gpu/GrDirectContext.h>
#include <skia/gpu/vk/VulkanExtensions.h>
#include <skia/core/SkPath.h>
#include <skia/core/SkFont.h>
#include <skia/core/SkRRect.h>
#include <skia/core/SkBitmap.h>
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkSurface.h>
#include <skia/core/SkFontMgr.h>
#include <skia/core/SkFontMetrics.h>
#include <skia/core/SkPathMeasure.h>
#include <skia/core/SkPathUtils.h>
#include <skia/utils/SkParsePath.h>
#include <skia/core/SkTextBlob.h>
#include <skia/effects/SkGradientShader.h>
#include <skia/effects/SkImageFilters.h>
#include <skia/effects/SkDashPathEffect.h>
#include <skia/core/SkStream.h>
#include <skia/modules/svg/include/SkSVGDOM.h>
#include <skia/modules/svg/include/SkSVGNode.h>
#include <skia/core/SkAlphaType.h>
#include <skia/core/SkColor.h>
#include <skia/core/SkColorType.h>
#include <skia/core/SkImageInfo.h>
#include <skia/core/SkRefCnt.h>
#include <skia/core/SkTypes.h>
#include <skia/gpu/GrDirectContext.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/vk/GrVkBackendContext.h>
#include <skia/gpu/vk/VulkanExtensions.h>
//#include <skia/tools/gpu/vk/VkTestUtils.h>

int a_cpp_func();
/*
static Skia *Context(VkEngine e) {
    static struct Skia *sk = null;
    if (sk) return sk;

    //GrBackendFormat gr_conv = GrBackendFormat::MakeVk(VK_FORMAT_R8G8B8_SRGB);
    Vulkan vk;
    GrVkBackendContext grc {
        vk->inst(), // vk instance (have this)
        e->vk_gpu->phys, // physical hardware id (i have this)
        e->vk_device->device, // device logical (have this)
        e->vk_device->graphicsQueue, // this is what i am unsure of
        e->vk_gpu->indices.graphicsFamily.value(), // this is the graphics family id (have tihs)
        vk->version //  i forget what format this is in
    };
    //grc.fVkExtensions -- not sure if we need to populate this with our extensions, but it has no interface to do so
    grc.fMaxAPIVersion = vk->version;


    //grc.fVkExtensions = new GrVkExtensions(); // internal needs population perhaps
    grc.fGetProc = [](cchar_t *name, VkInstance inst, VkDevice dev) -> PFN_vkVoidFunction {
        return (dev == VK_NULL_HANDLE) ? vkGetInstanceProcAddr(inst, name) :
                                            vkGetDeviceProcAddr  (dev,  name);
    };

    static sk_sp<GrDirectContext> ctx = GrDirectContext::MakeVulkan(grc);

    assert(ctx);
    sk = new Skia(ctx.get());
    return sk;
}
*/

extern "C" {
int something() {
    return a_cpp_func();
}
}