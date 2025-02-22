#include <vulkan/vulkan.h>
#include <tinyexr.h>
#include <png.h>
#include <import>

// read images without conversion; for .png and .exr
// this facilitates grayscale maps, environment color, 
// color maps, grayscale components for various PBR material attributes

none image_init(image a) {
    string ext = ext(a->uri);
    A header = head(a);
    symbol uri = (symbol)cstring(a->uri);

    if (eq(ext, "exr")) {
        EXRImage      exr_image;
        EXRHeader     exr_header;
        EXRVersion    exr_version;
        
        InitEXRImage (&exr_image);
        InitEXRHeader(&exr_header);
        f32*   data = (f32*)null;
        symbol err  = (symbol)null;
        int    ret  = ParseEXRHeaderFromFile(&exr_header, &exr_version, uri, &err);
        verify(ret == TINYEXR_SUCCESS, "error parsing EXR header: %s", err);
        ret = LoadEXRImageFromFile(&exr_image, &exr_header, uri, &err);
        verify(ret == TINYEXR_SUCCESS, "error loading EXR image: %s", err);

        a->width    = exr_image.width;
        a->height   = exr_image.height;
        a->channels = exr_image.num_channels;
        a->format   = Pixel_rgbaf32;
        header->count  = a->channels * a->width * a->height;
        header->scalar = typeid(f32);
        header->data   = (object)data;
        a->pixel_size  = sizeof(f32) * a->channels; /// needs to know channel count
        FreeEXRImage (&exr_image);
        FreeEXRHeader(&exr_header);
    } else if (eq(ext, "png")) {
        FILE* file = fopen(uri, "rb");
        verify (file, "could not open PNG: %o", a->uri);

        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        verify (png, "could not init PNG: %o", a->uri);

        png_infop info = png_create_info_struct(png);
        setjmp (png_jmpbuf(png));
        png_init_io   (png, file);
        png_read_info (png, info);

        a->width            = png_get_image_width  (png, info);
        a->height           = png_get_image_height (png, info);
        a->channels         = png_get_channels     (png, info);
        a->format           = Pixel_rgba8;
        png_byte bit_depth  = png_get_bit_depth    (png, info);
        png_byte color_type = png_get_color_type   (png, info);

        /// store the exact format read
        png_read_update_info (png, info);
        png_bytep* rows = (png_bytep*)malloc (sizeof(png_bytep) * a->height);
        u8*        data = (u8*)malloc(a->width * a->height * a->channels * (bit_depth / 8));
        for (int y = 0; y < a->height; y++) {
            rows[y] = data + (y * a->width * a->channels * (bit_depth / 8));
        }

        /// read-image-rows
        png_read_image(png, rows);
        free(rows);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);

        /// Store in header
        header->count  = a->width * a->height * a->channels;
        header->scalar = (bit_depth == 16) ? typeid(u16) : typeid(u8);
        header->data   = (object)data;
        a->pixel_size  = (bit_depth / 8) * a->channels;
    }
}

none image_destructor(image a) {
    A header = head(a);
    free(header->data);
}

num image_len(image a) {
    return a->height;
}

num image_byte_count(image a) {
    A f = head(a);
    return f->count * f->scalar->size;
}

object image_get(image a, num y) {
    i8* bytes = (i8*)data(a);
    return (object)&bytes[a->pixel_size * a->width * y];
}

define_class(image);

define_sentry(Zero,  0);
define_sentry(One,   1);
define_sentry(Two,   2);
define_sentry(Three, 3);
define_sentry(Four,  4);