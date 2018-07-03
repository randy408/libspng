#include "test_spng.h"
#include "test_png.h"
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

    struct spng_ihdr ihdr;
    size_t img_spng_size;
    unsigned char *img_spng;

    size_t img_png_size;
    unsigned char *img_png;

    struct timespec a, b;

    long nanoseconds;
    uint64_t avg_png = 0, avg_spng = 0;
    double percentage;

    int i, runs = 5;
    for(i=0; i < runs; i++)
    {
        timespec_get(&a, TIME_UTC);
        img_spng = getimage_libspng(pngbuf, siz_pngbuf, &img_spng_size, SPNG_FMT_RGBA8, 0, &ihdr);
        timespec_get(&b, TIME_UTC);

        if(img_spng == NULL)
        {
            printf("getimage_libspng() failed\n");
            return 1;
        }

        if ((b.tv_nsec - a.tv_nsec) < 0) nanoseconds = b.tv_nsec - a.tv_nsec + 1000000000;
        else nanoseconds = b.tv_nsec - a.tv_nsec;

        avg_spng += nanoseconds / 1000;

        timespec_get(&a, TIME_UTC);
        img_png = getimage_libpng(pngbuf, siz_pngbuf, &img_png_size, SPNG_FMT_RGBA8, 0);
        timespec_get(&b, TIME_UTC);

        if(img_png == NULL)
        {
            printf("getimage_libpng() failed\n");
            return 1;
        }

        if ((b.tv_nsec - a.tv_nsec) < 0) nanoseconds = b.tv_nsec - a.tv_nsec + 1000000000;
        else nanoseconds = b.tv_nsec - a.tv_nsec;

        avg_png += nanoseconds / 1000;

        free(img_spng);
        free(img_png);
    }

    avg_spng /= runs;
    avg_png /= runs;
    if(avg_spng < avg_png) percentage = (double)avg_png / (double)avg_spng * 100.0 - 100.0;
    else percentage = (double)avg_spng / (double)avg_png * 100.0 - 100.0;

    printf("spng average: %lu microseconds\npng average: %lu microseconds\n", avg_spng, avg_png);
    printf("spng is %.2f%% %s than libpng\n", percentage, avg_spng < avg_png ? "faster" : "slower");

    free(pngbuf);

    return 0;
}
