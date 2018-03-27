#ifndef TEST_VPNG_H
#define TEST_VPNG_H

#include "vpng.h"

#include <stdio.h>
#include <string.h>


unsigned char *getimage_libvpng(unsigned char *buf, size_t size, size_t *out_size, uint32_t *w, uint32_t *h)
{
    int r;
    size_t siz;
    unsigned char *out = NULL;
    struct vpng_ihdr ihdr;

    struct vpng_decoder *dec = vpng_decoder_new();

    if(dec==NULL)
    {
        printf("vpng_decoder_new() failed\n");
        return NULL;
    }

    r = vpng_decoder_set_buffer(dec, buf, size);

    if(r)
    {
        printf("vpng_decoder_set_buffer() returned %d\n", r);
        goto err;
    }

    r = vpng_get_ihdr(dec, &ihdr);

    if(r)
    {
        printf("vpng_get_ihdr() returned %d\n", r);
        goto err;
    }

    *w = ihdr.width;
    *h = ihdr.height;

#if defined(TEST_VPNG_IMG_INFO)
    printf("image info: %ux%u %u bpp, type %u, %s\n",
            ihdr.width, ihdr.height, ihdr.bit_depth, ihdr.colour_type,
            ihdr.interlace_method ? "interlaced" : "non-interlaced");
#endif

    r = vpng_get_output_image_size(dec, VPNG_FMT_RGBA8, &siz);
    if(r) goto err;

    *out_size = siz;

    out = malloc(siz);
    if(out==NULL) goto err;

    r = vpng_decode_image(dec, VPNG_FMT_RGBA8, out, siz, 0);

    if(r)
    {
        printf("vpng_get_image_rgba8() returned %d\n", r);
        goto err;
    }

    vpng_decoder_free(dec);

goto skip_err;

err:
    vpng_decoder_free(dec);
    if(out !=NULL) free(out);
    return NULL;

skip_err:

    return out;
}

#endif /* TEST_VPNG_H */
