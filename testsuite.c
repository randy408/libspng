#define TEST_VPNG_IMG_INFO
#define TEST_VPNG_EXACT_PIXELS
/*#define TEST_VPNG_STREAM_READ_INFO*/
/*#define TEST_VPNG_ANALYZE_MALLOC */

#include "test_vpng.h"
#include "test_png.h"

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
    int should_fail = 0;

    /* all images beginning with "x" are invalid */
    if(strstr(argv[1], "/x") != NULL) should_fail = 1;

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
    size_t img_vpng_size;
    unsigned char *img_vpng =  NULL;
    img_vpng = getimage_libvpng(pngbuf, siz_pngbuf, &img_vpng_size, &w, &h);
    if(img_vpng==NULL)
    {
        printf("getimage_libvpng() failed\n");
        return 1;
    }

    /* return 0 for should_fail tests if vpng can't detect an invalid file */
    if(should_fail) return 0;

    size_t img_png_size;
    unsigned char *img_png = NULL;
    img_png = getimage_libpng(pngbuf, siz_pngbuf, &img_png_size);
    if(img_png==NULL)
    {
        printf("getimage_libpng() failed\n");
        return 1;
    }

    if(img_png_size != img_vpng_size)
    {
        printf("output image size mismatch\n");
        printf("vpng: %zu\n png: %zu\n", img_vpng_size, img_png_size);
        free(pngbuf);
        free(img_vpng);
        free(img_png);
        return 1;
    }

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

            memcpy(&v_red, img_vpng + px_ofs, 1);
            memcpy(&v_green, img_vpng + px_ofs + 1, 1);
            memcpy(&v_blue, img_vpng + px_ofs + 2, 1);
            memcpy(&v_alpha, img_vpng + px_ofs + 3, 1);

            memcpy(&p_red, img_png + px_ofs, 1);
            memcpy(&p_green, img_png + px_ofs + 1, 1);
            memcpy(&p_blue, img_png + px_ofs + 2, 1);
            memcpy(&p_alpha, img_png + px_ofs + 3, 1);

            if(v_alpha != p_alpha)
            {
                printf("alpha mismatch at x:%u y:%u, vpng: %u png: %u\n", x, y, v_alpha, p_alpha);
                alpha_mismatch = 1;
            }

#if defined(TEST_VPNG_EXACT_PIXELS)
            if(v_red != p_red || v_green != p_green || v_blue != p_blue)
            {
                printf("color difference at x: %u y:%u, vpng: %u %u %u png: %u %u %u\n", x, y,
                       v_red, v_green, v_blue, p_red, p_green, p_blue);
                color_diff = 1;
            }
#endif
        }
    }

    int ret = 0;

    if(alpha_mismatch) ret = 1;
#if defined(TEST_VPNG_EXACT_PIXELS)
    if(color_diff) ret = 1;
#endif

    free(pngbuf);

    free(img_vpng);
    free(img_png);

    return ret;
}
