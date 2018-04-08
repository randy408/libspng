#define TEST_SPNG_IMG_INFO
#define TEST_SPNG_EXACT_PIXELS
/*#define TEST_SPNG_STREAM_READ_INFO*/
/*#define TEST_SPNG_ANALYZE_MALLOC */

#include "test_spng.h"
#include "test_png.h"

int should_fail = 0;

int compare_images8(uint8_t w, uint8_t h, unsigned char *img_spng, unsigned char *img_png)
{
    uint32_t x, y;
    uint8_t alpha_mismatch = 0;
    uint8_t color_diff = 0;

    for(y=0; y < h; y++)
    {
        for(x=0; x < w; x++)
        {
            uint8_t v_red, v_green, v_blue, v_alpha;
            uint8_t p_red, p_green, p_blue, p_alpha;

            size_t px_ofs = (x + (y * w)) * 4;

            memcpy(&v_red, img_spng + px_ofs, 1);
            memcpy(&v_green, img_spng + px_ofs + 1, 1);
            memcpy(&v_blue, img_spng + px_ofs + 2, 1);
            memcpy(&v_alpha, img_spng + px_ofs + 3, 1);

            memcpy(&p_red, img_png + px_ofs, 1);
            memcpy(&p_green, img_png + px_ofs + 1, 1);
            memcpy(&p_blue, img_png + px_ofs + 2, 1);
            memcpy(&p_alpha, img_png + px_ofs + 3, 1);

            if(v_alpha != p_alpha)
            {
                printf("alpha mismatch at x:%u y:%u, spng: %u png: %u\n", x, y, v_alpha, p_alpha);
                alpha_mismatch = 1;
            }

#if defined(TEST_SPNG_EXACT_PIXELS)
            if(v_red != p_red || v_green != p_green || v_blue != p_blue)
            {
                printf("color difference at x: %u y:%u, spng: %u %u %u png: %u %u %u\n", x, y,
                       v_red, v_green, v_blue, p_red, p_green, p_blue);
                color_diff = 1;
            }
#endif
        }
    }

    if(alpha_mismatch || color_diff) return 1;

    return 0;
}

int compare_images16(uint8_t w, uint8_t h, unsigned char *img_spng, unsigned char *img_png)
{
    uint32_t x, y;
    uint8_t alpha_mismatch = 0;
    uint8_t color_diff = 0;

    for(y=0; y < h; y++)
    {
        for(x=0; x < w; x++)
        {
            uint16_t v_red, v_green, v_blue, v_alpha;
            uint16_t p_red, p_green, p_blue, p_alpha;

            size_t px_ofs = (x + (y * w)) * 8;

            memcpy(&v_red, img_spng + px_ofs, 2);
            memcpy(&v_green, img_spng + px_ofs + 2, 2);
            memcpy(&v_blue, img_spng + px_ofs + 4, 2);
            memcpy(&v_alpha, img_spng + px_ofs + 6, 2);

            memcpy(&p_red, img_png + px_ofs, 2);
            memcpy(&p_green, img_png + px_ofs + 2, 2);
            memcpy(&p_blue, img_png + px_ofs + 4, 2);
            memcpy(&p_alpha, img_png + px_ofs + 6, 2);

            if(v_alpha != p_alpha)
            {
                printf("alpha mismatch at x:%u y:%u, spng: %u png: %u\n", x, y, v_alpha, p_alpha);
                alpha_mismatch = 1;
            }

#if defined(TEST_SPNG_EXACT_PIXELS)
            if(v_red != p_red || v_green != p_green || v_blue != p_blue)
            {
                printf("color difference at x: %u y:%u, spng: %u %u %u png: %u %u %u\n", x, y,
                       v_red, v_green, v_blue, p_red, p_green, p_blue);
                color_diff = 1;
            }
#endif
        }
    }

    if(alpha_mismatch || color_diff) return 1;

    return 0;
}

int decode_and_compare(unsigned char *pngbuf, size_t siz_pngbuf, int fmt, int flags)
{
    struct spng_ihdr ihdr;
    uint32_t w, h;
    size_t img_spng_size;
    unsigned char *img_spng =  NULL;
    img_spng = getimage_libspng(pngbuf, siz_pngbuf, &img_spng_size, fmt, flags, &ihdr);
    if(img_spng==NULL)
    {
        printf("getimage_libspng() failed\n");
        return 1;
    }

    /* return 0 for should_fail tests if spng can't detect an invalid file */
    if(should_fail)
    {
        free(img_spng);
        return 0;
    }

    w = ihdr.width;
    h = ihdr.height;

    size_t img_png_size;
    unsigned char *img_png = NULL;
    img_png = getimage_libpng(pngbuf, siz_pngbuf, &img_png_size, fmt, flags);
    if(img_png==NULL)
    {
        printf("getimage_libpng() failed\n");
        free(img_spng);
        return 1;
    }

    if(img_png_size != img_spng_size)
    {
        printf("output image size mismatch\n");
        printf("spng: %zu\n png: %zu\n", img_spng_size, img_png_size);
        free(img_spng);
        free(img_png);
        return 1;
    }

    int ret=0;
    if(fmt == SPNG_FMT_RGBA8) ret = compare_images8(w, h, img_spng, img_png);
    else if(fmt == SPNG_FMT_RGBA16) ret = compare_images16(w, h, img_spng, img_png);

    free(img_spng);
    free(img_png);

    return ret;
}

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        printf("no input file\n");
        return 1;
    }

    FILE *png;
    unsigned char *pngbuf;
    char *filename = argv[1];
    png = fopen(filename, "r");

    /* all images beginning with "x" are invalid */
    if(strstr(filename, "/x") != NULL) should_fail = 1;

    if(png==NULL)
    {
        printf("error opening input file %s\n", filename);
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

    int ret=0;

    ret = decode_and_compare(pngbuf, siz_pngbuf, SPNG_FMT_RGBA8, 0);
    printf("decode and compare FMT_RGBA8: %s\n", ret ? "fail" : "ok");

/*    ret = decode_and_compare(pngbuf, siz_pngbuf, SPNG_FMT_RGBA16, 0);
    printf("decode and compare FMT_RGBA16: %s", ret ? "fail" : "ok");*/

    free(pngbuf);

    return ret;
}
