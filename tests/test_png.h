#ifndef TEST_PNG_H
#define TEST_PNG_H

#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(__BIG_ENDIAN__)
    #define SPNG_BIG_ENDIAN
#else
    #define SPNG_LITTLE_ENDIAN
#endif

#include <png.h>
#include "test_spng.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

unsigned char *getimage_libpng(FILE *file, size_t *out_size, int fmt, int flags, png_infop *iptr, png_structp *pptr)
{
    png_infop info_ptr;
    png_structp png_ptr;

    unsigned char *image = NULL;
    png_bytep *row_pointers = NULL;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(png_ptr == NULL)
    {
        printf("libpng init failed\n");
        return NULL;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if(info_ptr == NULL)
    {
        printf("png_create_info_struct failed\n");
        return NULL;
    }

    if(setjmp(png_jmpbuf(png_ptr)))
    {
        printf("libpng error\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        if(image != NULL) free(image);
        if(row_pointers != NULL) free(row_pointers);
        return NULL;
    }

    png_init_io(png_ptr, file);

    png_read_info(png_ptr, info_ptr);

    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type, compression_type;
    int filter_method;

    if(!png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
                     &color_type, &interlace_type, &compression_type, &filter_method))
    {
        printf("png_get_IHDR failed\n");
        return NULL;
    }

    if(flags & SPNG_DECODE_GAMMA)
    {
        double gamma;
        if(png_get_gAMA(png_ptr, info_ptr, &gamma))
            png_set_gamma(png_ptr, 2.2, gamma);
    }

    if(fmt == SPNG_FMT_RGBA16)
    {
        png_set_gray_to_rgb(png_ptr);

        png_set_filler(png_ptr, 0xFFFF, PNG_FILLER_AFTER);

        /* png_set_palette_to_rgb() + png_set_tRNS_to_alpha() */
        png_set_expand_16(png_ptr);
    }
    else if(fmt == SPNG_FMT_RGBA8)
    {
        png_set_gray_to_rgb(png_ptr);

        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

        /* png_set_palette_to_rgb() + png_set_expand_gray_1_2_4_to_8() + png_set_tRNS_to_alpha() */
        png_set_expand(png_ptr);

        png_set_strip_16(png_ptr);
    }
    else if(fmt == SPNG_FMT_RGB8)
    {
        png_set_gray_to_rgb(png_ptr);

        png_set_strip_alpha(png_ptr);

        png_set_strip_16(png_ptr);
    }
    else if(fmt == SPNG_FMT_G8) /* assumes only <=8-bit grayscale images */
    {/* TODO: support all input formats */
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }
    else if(fmt == SPNG_FMT_GA16) /* assumes only 16-bit grayscale images */
    {/* TODO: support all input formats */
        if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) && (flags & SPNG_DECODE_TRNS))
        {
            png_set_tRNS_to_alpha(png_ptr);
        }
        else
        {
            if(bit_depth == 16) png_set_filler(png_ptr, 0xFFFF, PNG_FILLER_AFTER);
        }
    }
    else if(fmt == SPNG_FMT_GA8) /* assumes only <=8-bit grayscale images */
    {/* TODO: support all input formats */
        if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) && (flags & SPNG_DECODE_TRNS))
        {
            png_set_tRNS_to_alpha(png_ptr);
        }
        else
        {
            png_set_expand_gray_1_2_4_to_8(png_ptr);
            png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
        }
    }
    else if(fmt == SPNGT_FMT_VIPS)
    {
        if(color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);

        if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_ptr);

        if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);

#if defined(SPNG_LITTLE_ENDIAN)
         png_set_swap(png_ptr);
#endif
    }

#if defined(SPNG_LITTLE_ENDIAN) /* we want host-endian values unless it's SPNG_FMT_RAW */
    if(fmt != SPNG_FMT_RAW) png_set_swap(png_ptr);
#endif

    png_set_interlace_handling(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);

    size_t image_size = height * rowbytes;
    memcpy(out_size, &image_size, sizeof(size_t));

    /* Neither library does zero-padding for <8-bit images,
       but we want the images to be bit-identical for memcmp() */
    image = calloc(1, image_size);
    if(image == NULL)
    {
         printf("libpng: malloc() failed\n");
         png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
         return NULL;
    }

    row_pointers = malloc(height * sizeof(png_bytep));
    if(row_pointers == NULL)
    {
        printf("libpng: malloc() failed\n");
        free(image);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    int k;
    for(k=0; k < height; k++)
    {
        row_pointers[k] = image + k * rowbytes;
    }

    png_read_image(png_ptr, row_pointers);

    png_read_end(png_ptr, info_ptr);

    free(row_pointers);

    *pptr = png_ptr;
    *iptr = info_ptr;

    return image;
}

#endif /* TEST_PNG_H */
