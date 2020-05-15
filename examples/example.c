#include <spng.h>

#include <inttypes.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    int r = 0;
    FILE *png;
    spng_ctx *ctx = NULL;
    unsigned char *out = NULL;

    if(argc < 2)
    {
        printf("no input file\n");
        goto err;
    }

    png = fopen(argv[1], "rb");
    if(png == NULL)
    {
        printf("error opening input file %s\n", argv[1]);
        goto err;
    }

    ctx = spng_ctx_new(0);
    if(ctx == NULL)
    {
        printf("spng_ctx_new() failed\n");
        goto err;
    }

    spng_set_crc_action(ctx, SPNG_CRC_USE, SPNG_CRC_USE);

    spng_set_png_file(ctx, png);

    struct spng_ihdr ihdr;
    r = spng_get_ihdr(ctx, &ihdr);

    if(r)
    {
        printf("spng_get_ihdr() error: %s\n", spng_strerror(r));
        goto err;
    }

    char *clr_type_str;
    if(ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE)
        clr_type_str = "grayscale";
    else if(ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR)
        clr_type_str = "truecolor";
    else if(ihdr.color_type == SPNG_COLOR_TYPE_INDEXED)
        clr_type_str = "indexed color";
    else if(ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE_ALPHA)
        clr_type_str = "grayscale with alpha";
    else
        clr_type_str = "truecolor with alpha";

    printf("width: %" PRIu32 "\nheight: %" PRIu32 "\n"
           "bit depth: %" PRIu8 "\ncolor type: %" PRIu8 " - %s\n",
           ihdr.width, ihdr.height,
           ihdr.bit_depth, ihdr.color_type, clr_type_str);
    printf("compression method: %" PRIu8 "\nfilter method: %" PRIu8 "\n"
           "interlace method: %" PRIu8 "\n",
           ihdr.compression_method, ihdr.filter_method,
           ihdr.interlace_method);

    size_t out_size, out_width;

    /* Output format, does not depend on source PNG format except for
       SPNG_FMT_PNG, which is the PNG's format in host-endian and SPNG_FMT_RAW
       where the PNG's data is left in byte-packed or big endian format */
    int fmt = SPNG_FMT_PNG;

    /* For this format indexed color images are output as palette indices,
       if you want to expand them pick another format */
    if(ihdr.color_type == SPNG_COLOR_TYPE_INDEXED) fmt = SPNG_FMT_RGB8;

    r = spng_decoded_image_size(ctx, fmt, &out_size);

    if(r) goto err;

    out = malloc(out_size);
    if(out == NULL) goto err;

    /* This is required to initialize for progressive decoding */
    r = spng_decode_image(ctx, NULL, 0, fmt, SPNG_DECODE_PROGRESSIVE);
    if(r)
    {
        printf("progressive spng_decode_image() error: %s\n", spng_strerror(r));
        goto err;
    }

    /* ihdr.height will always be non-zero if spng_get_ihdr() succeeds */
    out_width = out_size / ihdr.height;

    struct spng_row_info row_info = {0};

    do
    {
        r = spng_get_row_info(ctx, &row_info);
        if(r) break;

        r = spng_decode_row(ctx, out + row_info.row_num * out_width, out_width);
    }
    while(!r);

    if(r != SPNG_EOI)
    {
        printf("progressive decode error: %s\n", spng_strerror(r));
    }

    /* r = spng_decode_image(ctx, out, out_size, SPNG_FMT_RGBA8, 0);

    if(r)
    {
        printf("spng_decode_image() error: %s\n", spng_strerror(r));
        goto err;
    } */

err:
    spng_ctx_free(ctx);
    free(out);

    return r;
}
