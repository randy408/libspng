#define SPNG_UNTESTED
#include "../spng.h"

#include <string.h>

struct buf_state
{
    const uint8_t *data;
    size_t bytes_left;
};

static int buffer_read_fn(spng_ctx *ctx, void *user, void *dest, size_t length)
{
    struct buf_state *state = (struct buf_state*)user;

    if(length > state->bytes_left) return SPNG_IO_EOF;

    memcpy(dest, state->data, length);

    state->bytes_left -= length;
    state->data += length;

    return 0;
}

#ifdef __cplusplus
extern "C"
#endif
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if(size < 2) return 0;

    int flags = data[0];
    int fmt = data[1] & 0x7F;
    int stream = data[1] >> 7;

    data+=2; size-=2;

    int ret;
    unsigned char *out = NULL;

    spng_ctx *ctx = spng_ctx_new(SPNG_CTX_IGNORE_ADLER32);
    if(ctx == NULL) return 0;

    struct buf_state state;
    state.data = data;
    state.bytes_left = size;

    if(stream) ret = spng_set_png_stream(ctx, buffer_read_fn, &state);
    else ret = spng_set_png_buffer(ctx, (void*)data, size);

    if(ret) goto err;

    spng_set_image_limits(ctx, 200000, 200000);

    spng_set_chunk_limits(ctx, 4 * 1000 * 1000, 8 * 1000 * 1000);

    spng_set_crc_action(ctx, SPNG_CRC_USE, SPNG_CRC_USE);

    size_t out_size;
    if(spng_decoded_image_size(ctx, fmt, &out_size)) goto err;

    if(out_size > 80000000) goto err;

    out = (unsigned char*)malloc(out_size);
    if(out == NULL) goto err;

    if(spng_decode_image(ctx, out, out_size, fmt, flags)) goto err;

err:
    spng_ctx_free(ctx);
    if(out != NULL) free(out);

    return 0;
}
