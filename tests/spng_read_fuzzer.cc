#define SPNG_UNTESTED
#include "../spng.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    unsigned char *out = NULL;
    int flags;

    spng_ctx *ctx = spng_ctx_new(0);
    if(ctx == NULL) return 0;

    if(spng_set_png_buffer(ctx, (void*)data, size)) goto err;

    spng_set_image_limits(ctx, 200000, 200000);
    
    spng_set_chunk_limits(ctx, 4 * 1000 * 1000, 8 * 1000 * 1000);

    spng_set_crc_action(ctx, SPNG_CRC_USE, SPNG_CRC_USE);

    size_t out_size;
    if(spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &out_size)) goto err;

    if(out_size > 80000000) goto err;

    out = (unsigned char*)malloc(out_size);
    if(out == NULL) goto err;

    flags = SPNG_DECODE_USE_TRNS | SPNG_DECODE_USE_GAMA | SPNG_DECODE_USE_SBIT;
    if(spng_decode_image(ctx, out, out_size, SPNG_FMT_RGBA8, flags)) goto err;

err:
    spng_ctx_free(ctx);
    if(out != NULL) free(out);

    return 0;
}
