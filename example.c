#include "vpng.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    char *in_name = "a.png";
    FILE *png;
    char *pngbuf;
    png = fopen(in_name, "r");
    if(png==NULL)
    {
        printf("error opening input file %s\n", in_name);
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

    char *out_name = "a.data";
    FILE *raw = fopen(out_name, "wb");

    if(raw==NULL) printf("error opening output file %s\n", out_name);

    fwrite(out, out_size, 1, raw);

    fclose(raw);

    free(out);
    free(pngbuf);

    return 0;
}
