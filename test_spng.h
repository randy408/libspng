#ifndef TEST_SPNG_H
#define TEST_SPNG_H

#include "spng.h"

#include <stdio.h>
#include <string.h>

unsigned char *getimage_libspng(unsigned char *buf, size_t size, size_t *out_size, int fmt, int flags, struct spng_ihdr *info)
{
    int r;
    size_t siz;
    unsigned char *out = NULL;
    struct spng_ihdr ihdr;

    struct spng_decoder *dec = spng_decoder_new();

    if(dec==NULL)
    {
        printf("spng_decoder_new() failed\n");
        return NULL;
    }

    r = spng_decoder_set_buffer(dec, buf, size);

    if(r)
    {
        printf("spng_decoder_set_buffer() returned %d\n", r);
        goto err;
    }

    r = spng_get_ihdr(dec, &ihdr);

    if(r)
    {
        printf("spng_get_ihdr() returned %d\n", r);
        goto err;
    }

    memcpy(info, &ihdr, sizeof(struct spng_ihdr));

#if defined(TEST_SPNG_IMG_INFO)
    printf("image info: %ux%u %u bits per sample, type %u, %s\n",
            ihdr.width, ihdr.height, ihdr.bit_depth, ihdr.colour_type,
            ihdr.interlace_method ? "interlaced" : "non-interlaced");
#endif

    r = spng_get_output_image_size(dec, fmt, &siz);
    if(r) goto err;

    *out_size = siz;

    out = malloc(siz);
    if(out==NULL) goto err;

    r = spng_decode_image(dec, fmt, out, siz, 0);

    if(r)
    {
        printf("spng_get_image_rgba8() returned %d\n", r);
        goto err;
    }

    spng_decoder_free(dec);

goto skip_err;

err:
    spng_decoder_free(dec);
    if(out !=NULL) free(out);
    return NULL;

skip_err:

    return out;
}

#endif /* TEST_SPNG_H */
