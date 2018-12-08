#include "spng.h"

#include <inttypes.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        printf("no input file\n");
        return 1;
    }

    FILE *png;
    char *pngbuf;
    png = fopen(argv[1], "r");

    if(png==NULL)
    {
        printf("error opening input file %s\n", argv[1]);
        return 1;
    }

    fseek(png, 0, SEEK_END);

    long siz_pngbuf = ftell(png);
    rewind(png);

    if(siz_pngbuf < 1) return 1;

    pngbuf = malloc(siz_pngbuf);
    if(pngbuf==NULL)
    {
        printf("malloc() failed\n");
        return 1;
    }

    if(fread(pngbuf, siz_pngbuf, 1, png) != 1)
    {
        printf("fread() failed\n");
        return 1;
    }

    spng_ctx *ctx = spng_ctx_new(0);
    if(ctx == NULL)
    {
        printf("spng_ctx_new() failed\n");
        return 1;
    }

    int r;
    r = spng_set_png_buffer(ctx, pngbuf, siz_pngbuf);

    if(r)
    {
        printf("spng_set_png_buffer() error: %s\n", spng_strerror(r));
        return 1;
    }

    struct spng_ihdr ihdr;
    r = spng_get_ihdr(ctx, &ihdr);

    if(r)
    {
        printf("spng_get_ihdr() error: %s\n", spng_strerror(r));
        return 1;
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

    size_t out_size;

    r = spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &out_size);

    if(r) return 1;

    unsigned char *out = malloc(out_size);
    if(out==NULL) return 1;

    r = spng_decode_image(ctx, out, out_size, SPNG_FMT_RGBA8, 0);

    if(r)
    {
        printf("spng_decode_image() error: %s\n", spng_strerror(r));
        return 1;
    }

    spng_ctx_free(ctx);

    /* write raw pixels to file */
    char *out_name = "image.data";
    FILE *raw = fopen(out_name, "wb");

    if(raw==NULL) printf("error opening output file %s\n", out_name);

    fwrite(out, out_size, 1, raw);

    fclose(raw);

    free(out);
    free(pngbuf);

    return 0;
}