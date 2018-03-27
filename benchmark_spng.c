#include "test_spng.h"
#include <time.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        printf("no input file\n");
        return 1;
    }

    FILE *png;
    unsigned char *pngbuf;
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

    uint32_t w, h;
    size_t img_spng_size;
    unsigned char *img_spng;

    struct timespec a, b;

    timespec_get(&a, TIME_UTC);

    img_spng = getimage_libspng(pngbuf, siz_pngbuf, &img_spng_size, &w, &h);

    timespec_get(&b, TIME_UTC);

    if(img_spng == NULL)
    {
        printf("getimage_libspng() failed\n");
        return 1;
    }

    long nanoseconds;

    if ((b.tv_nsec - a.tv_nsec) < 0) nanoseconds = b.tv_nsec - a.tv_nsec + 1000000000;
    else nanoseconds = b.tv_nsec - a.tv_nsec;

    printf("time: %lu microseconds\n", nanoseconds / 1000);

    free(img_spng);

    return 0;
}
