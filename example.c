#include "vpng.h"

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

    pngbuf = malloc(siz_pngbuf);
    fread(pngbuf, siz_pngbuf, 1, png);

    if(pngbuf==NULL)
    {
        printf("malloc() failed\n");
        return 1;
    }

    struct vpng_decoder *dec = vpng_decoder_new();
    if(dec==NULL)
    {
        printf("vpng_decoder_new() failed\n");
        return 1;
    }

    int r;
    r = vpng_decoder_set_buffer(dec, pngbuf, siz_pngbuf);

    if(r)
    {
        printf("vpng_decoder_set_buffer() returned %d\n", r);
        return 1;
    }

    struct vpng_ihdr ihdr;
    r = vpng_get_ihdr(dec, &ihdr);

    if(r)
    {
        printf("vpng_get_ihdr() returned %d\n", r);
        return 1;
    }

    char *clr_type_str;
    if(ihdr.colour_type == VPNG_COLOUR_TYPE_GRAYSCALE)
        clr_type_str = "grayscale";
    else if(ihdr.colour_type == VPNG_COLOUR_TYPE_TRUECOLOR)
        clr_type_str = "truecolor";
    else if(ihdr.colour_type == VPNG_COLOUR_TYPE_INDEXED_COLOUR)
        clr_type_str = "indexed colour";
    else if(ihdr.colour_type == VPNG_COLOUR_TYPE_GRAYSCALE_WITH_ALPHA)
        clr_type_str = "grayscale with alpha";
    else
        clr_type_str = "truecolor with alpha";

    printf("width: %u\nheight: %u\nbit depth: %u\ncolour type: %u - %s\n",
            ihdr.width, ihdr.height, ihdr.bit_depth, ihdr.colour_type, clr_type_str);
    printf("compression method: %u\nfilter method: %u\ninterlace method: %u\n",
            ihdr.compression_method, ihdr.filter_method, ihdr.interlace_method);

    size_t out_size;

    r = vpng_get_output_image_size(dec, VPNG_FMT_RGBA8, &out_size);

    if(r) return 1;

    unsigned char *out = malloc(out_size);
    if(out==NULL) return 1;

    r = vpng_get_image_rgba8(dec, out, out_size);

    if(r)
    {
        printf("vpng_get_image_rgba8() returned %d\n", r);
        return 1;
    }

    vpng_decoder_free(dec);

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
