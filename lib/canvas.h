#ifndef _CANVAS_
#define _CANVAS_

typedef void* skia_t;
typedef void* handle_t;
#ifdef __cplusplus
extern "C" {
#endif

skia_t skia_init_vk(handle_t vk_instance, handle_t phys, handle_t device, handle_t queue, unsigned int graphics_family, unsigned int vk_version);

#ifdef __cplusplus
}
#endif
#endif