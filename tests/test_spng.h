#ifndef TEST_SPNG_H
#define TEST_SPNG_H

#include <spng.h>

#include <stdio.h>
#include <string.h>

unsigned char *getimage_libspng(FILE *file, size_t *out_size, int fmt, int flags, struct spng_ihdr *info)
{
    int r;
    size_t siz;
    unsigned char *out = NULL;
    struct spng_ihdr ihdr;

    spng_ctx *ctx = spng_ctx_new(0);

    if(ctx == NULL)
    {
        printf("spng_ctx_new() failed\n");
        return NULL;
    }

    r = spng_set_png_file(ctx, file);

    if(r)
    {
        printf("spng_set_png_stream() error: %s\n", spng_strerror(r));
        goto err;
    }

    r = spng_set_image_limits(ctx, 16000, 16000);

    if(r)
    {
        printf("spng_set_image_limits() error: %s\n", spng_strerror(r));
        goto err;
    }

    r = spng_set_chunk_limits(ctx, 66 * 1000 * 1000, 66 * 1000* 1000);

    if(r)
    {
        printf("spng_set_chunk_limits() error: %s\n", spng_strerror(r));
        goto err;
    }

    r = spng_get_ihdr(ctx, &ihdr);

    if(r)
    {
        printf("spng_get_ihdr() error: %s\n", spng_strerror(r));
        goto err;
    }

    memcpy(info, &ihdr, sizeof(struct spng_ihdr));

    r = spng_decoded_image_size(ctx, fmt, &siz);
    if(r) goto err;

    *out_size = siz;

    /* Neither library does zero-padding for <8-bit images,
    but we want the images to be bit-identical for memcmp() */
    out = calloc(1, siz);
    if(out == NULL) goto err;

    r = spng_decode_image(ctx, out, siz,  fmt, flags);

    if(r)
    {
        printf("spng_decode_image() error: %s\n", spng_strerror(r));
        goto err;
    }

    spng_ctx_free(ctx);

goto skip_err;

err:
    spng_ctx_free(ctx);
    if(out != NULL) free(out);
    return NULL;

skip_err:

    return out;
}

#endif /* TEST_SPNG_H */
