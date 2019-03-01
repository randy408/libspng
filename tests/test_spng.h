#ifndef TEST_SPNG_H
#define TEST_SPNG_H

#define SPNG_UNTESTED

#include <spng.h>

#include <stdio.h>
#include <string.h>

struct read_fn_state
{
    unsigned char *data;
    size_t bytes_left;
};

int read_fn(struct spng_ctx *ctx, void *user, void *data, size_t n)
{
    struct read_fn_state *state = user;
    if(n > state->bytes_left) return SPNG_IO_EOF;

    unsigned char *dst = data;
    unsigned char *src = state->data;

#if defined(TEST_SPNG_STREAM_READ_INFO)
    printf("libspng bytes read: %lu\n", n);
#endif

    memcpy(dst, src, n);

    state->bytes_left -= n;
    state->data += n;

    return 0;
}

unsigned char *getimage_libspng(unsigned char *buf, size_t size, size_t *out_size, int fmt, int flags, struct spng_ihdr *info)
{
    int r;
    size_t siz;
    unsigned char *out = NULL;
    struct spng_ihdr ihdr;

    spng_ctx *ctx = spng_ctx_new(0);

    if(ctx==NULL)
    {
        printf("spng_ctx_new() failed\n");
        return NULL;
    }

    struct read_fn_state state;
    state.data = buf;
    state.bytes_left = size;

    r = spng_set_png_stream(ctx, read_fn, &state);

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

/*    r = spng_set_png_buffer(dec, buf, size);

    if(r)
    {
        printf("spng_set_png_buffer() error: %s\n", spng_strerror(r));
        goto err;
    }*/

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

    out = malloc(siz);
    if(out==NULL) goto err;

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
    if(out !=NULL) free(out);
    return NULL;

skip_err:

    return out;
}

#endif /* TEST_SPNG_H */
