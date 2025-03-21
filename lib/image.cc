#include <vulkan/vulkan.h>
#include <tinyexr.h>
#include <png.h>
#include <import>
#include <immintrin.h>

// read images without conversion; for .png and .exr
// this facilitates grayscale maps, environment color, 
// color maps, grayscale components for various PBR material attributes

image image_copy(image a) {
    image res = image(width, a->width, height, a->height, format, a->format, surface, a->surface);
    memcpy(data(res), data(a), byte_count(a));
    return res;
}

static f32 floorf(f32 f) {
    i32 i = (i32)f;
    return (f32)((f < 0 && f != i) ? i - 1 : i);
}

static f32 ceilf(f32 f) {
    i32 i = (i32)f;
    return (f > i) ? i + 1.0f : f;
}

static f32 roundf(f32 in) {
    i32 intPart = (i32)in;
    f32 decimalPart = in - intPart;
    
    if (decimalPart >= 0.5f)
        return intPart + 1.0f;
    else if (decimalPart <= -0.5f)
        return intPart - 1.0f;
    
    return intPart;
}

void opencv_resize_area(uint8_t* src, uint8_t* dst, int in_w, int in_h, int out_w, int out_h);

image image_resize(image input, i32 out_w, i32 out_h) {
    verify(input->width > 0 && input->height, "null image given to resize");
    verify(out_w > 0 && out_h > 0, "invalid size");
    
    int in_w = input->width;
    int in_h = input->height;

    if (in_w == out_w && in_h == out_h)
        return (image)copy((object)input);
    
    float scale_x = (float)in_w / out_w;
    float scale_y = (float)in_h / out_h;
    image output = image(width, out_w, height, out_h, format, input->format, surface, input->surface, channels, input->channels);
    
    u8* src = (u8*)A_data((A)input);
    u8* dst = (u8*)A_data((A)output);

    opencv_resize_area(src, dst, in_w, in_h, out_w, out_h);
    // lets call opencv's imgproc with average; check against our approach



    /*
    for (int y = 0; y < out_h; y++) {
        float start_y = (float)y / out_h * in_h;
        float to_y = (float)(y + 1) / out_h * in_h;

        for (int x = 0; x < out_w; x++) {
            float start_x = (float)x / out_w * in_w;
            float to_x = (float)(x + 1) / out_w * in_w;

            int sx = (int)floorf(start_x);
            int sy = (int)floorf(start_y);
            int ex = (int)ceilf(to_x);
            int ey = (int)ceilf(to_y);

            float value = 0;
            float weight = 0;

            // Top-Left (TL)
            float tl_weight = (ceilf(start_x) - start_x) * (ceilf(start_y) - start_y);
            if (sx >= 0 && sy >= 0) {
                value += src[sy * in_w + sx] * tl_weight;
                weight += tl_weight;
            }

            // Top-Right (TR)
            float tr_weight = (to_x - floorf(to_x)) * (ceilf(start_y) - start_y);
            if (ex < in_w && sy >= 0) {
                value += src[sy * in_w + ex] * tr_weight;
                weight += tr_weight;
            }

            // Bottom-Left (BL)
            float bl_weight = (ceilf(start_x) - start_x) * (to_y - floorf(to_y));
            if (sx >= 0 && ey < in_h) {
                value += src[ey * in_w + sx] * bl_weight;
                weight += bl_weight;
            }

            // Bottom-Right (BR)
            float br_weight = (to_x - floorf(to_x)) * (to_y - floorf(to_y));
            if (ex < in_w && ey < in_h) {
                value += src[ey * in_w + ex] * br_weight;
                weight += br_weight;
            }

            // Top Edge
            for (int tx = sx + 1; tx < ex; tx++) {
                float t_weight = (ceilf(start_y) - start_y);
                if (sy >= 0) {
                    value += src[sy * in_w + tx] * t_weight;
                    weight += t_weight;
                }
            }

            // Bottom Edge
            for (int bx = sx + 1; bx < ex; bx++) {
                float b_weight = (to_y - floorf(to_y));
                if (ey < in_h) {
                    value += src[ey * in_w + bx] * b_weight;
                    weight += b_weight;
                }
            }

            // Left Edge
            for (int ly = sy + 1; ly < ey; ly++) {
                float l_weight = (ceilf(start_x) - start_x);
                if (sx >= 0) {
                    value += src[ly * in_w + sx] * l_weight;
                    weight += l_weight;
                }
            }

            // Right Edge
            for (int ry = sy + 1; ry < ey; ry++) {
                float r_weight = (to_x - floorf(to_x));
                if (ex < in_w) {
                    value += src[ry * in_w + ex] * r_weight;
                    weight += r_weight;
                }
            }

            // Middle Pixels
            for (int my = sy + 1; my < ey; my++) {
                for (int mx = sx + 1; mx < ex; mx++) {
                    if (mx < in_w && my < in_h) {
                        value += src[my * in_w + mx];
                        weight += 1.0f;
                    }
                }
            }

            dst[y * out_w + x] = (u8)roundf(value / weight);
        }
    }
    */
    return output;
}


image image_with_string(image a, string i) {
    a->uri = path(i);
    return a;
}

image image_with_symbol(image a, symbol i) {
    a->uri = path(i);
    return a;
}

image image_with_cstr(image a, cstr i) {
    a->uri = path((symbol)i);
    return a;
}

none image_init(image a) {
    A header = head(a);

    if (!a->uri) {
        Pixel f = a->format;
        AType pixel_type =
            f == Pixel_none ? typeid(i8) : f == Pixel_rgba8   ? typeid(rgba8) : f == Pixel_rgbf32 ? typeid(vec3f) :
            f == Pixel_u8   ? typeid(i8) : f == Pixel_rgbaf32 ? typeid(vec4f) : typeid(f32);
        /// validate with channels if set?
        header->data = A_alloc(pixel_type, pixel_type->size * a->width * a->height, false);
        return;
    }

    string ext = ext(a->uri);
    symbol uri = (symbol)a->uri->chars;
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
        i64 plane_size = a->height * a->width;
        f32* planes = (f32*)calloc(sizeof(f32), plane_size * a->channels);
        for (int i = 0; i < a->channels; i++) {
            data = (f32*) exr_image.images[i];  // EXR stores planes separately
            i64 iplane = plane_size * i;
            memcpy(&planes[iplane], data, plane_size * sizeof(f32));
        }
        header->data   = (object)planes;  // <---- GPT is this right lol
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

// save gray or colored png based on channel count; if we want conversion we may just use methods to alter an object
i32 image_png(image a, path uri) {
    string e = ext(uri);
    if (eq(e, "png")) {
        /// lets support libpng only, so only "png" ext from path
        FILE* file = fopen(cstring(uri), "wb");
        verify(file, "could not open file for writing: %o", uri);

        png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        verify(png, "could not create PNG write struct");

        png_infop info = png_create_info_struct(png);
        verify(info, "could not create PNG info struct");

        if (setjmp(png_jmpbuf(png))) {
            png_destroy_write_struct(&png, &info);
            fclose(file);
            return false;
        }

        png_init_io(png, file);
        png_set_IHDR(
            png, info,
            a->width, a->height,
            8,  // 8-bit per channel
            (a->channels == 1) ? PNG_COLOR_TYPE_GRAY :
            (a->channels == 3) ? PNG_COLOR_TYPE_RGB :
            (a->channels == 4) ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_GRAY_ALPHA,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT
        );

        png_write_info(png, info);

        png_bytep* rows = (png_bytep*)malloc(sizeof(png_bytep) * a->height);
        u8* data = (u8*)data(a);
        for (int y = 0; y < a->height; y++) {
            rows[y] = data + y * a->width * a->channels;
        }

        png_write_image(png, rows);
        png_write_end(png, NULL);

        free(rows);
        png_destroy_write_struct(&png, &info);
        fclose(file);

    } else {
        fault("unsupported format: %o", e);
    }
    return 1;
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