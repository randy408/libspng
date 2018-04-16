/*#define TEST_SPNG_STREAM_READ_INFO*/
/*#define TEST_SPNG_ANALYZE_MALLOC */

#include "test_spng.h"
#include "test_png.h"

int should_fail = 0;
int info_printed = 0;

int compare_images(struct spng_ihdr *ihdr, int fmt, unsigned char *img_spng, unsigned char *img_png)
{
    uint32_t w, h;
    uint32_t x, y;
    uint8_t have_alpha = 1;
    uint8_t alpha_mismatch = 0;
    uint8_t pixel_diff = 0;
    uint8_t bytes_per_pixel = 4; /* SPNG_FMT_RGBA8 */

    uint16_t spng_red = 0, spng_green = 0, spng_blue = 0, spng_alpha = 0;
    uint16_t png_red = 0, png_green = 0, png_blue = 0, png_alpha = 0;

    w = ihdr->width;
    h = ihdr->height;

    if(fmt == SPNG_FMT_RGBA8)
    {
        bytes_per_pixel = 4;
    }
    else if(fmt == SPNG_FMT_RGBA16)
    {
        bytes_per_pixel = 8;
    }

    for(y=0; y < h; y++)
    {
        for(x=0; x < w; x++)
        {
            if(fmt == SPNG_FMT_RGBA16)
            {
                uint16_t s_red, s_green, s_blue, s_alpha;
                uint16_t p_red, p_green, p_blue, p_alpha;

                size_t px_ofs = (x + (y * w)) * bytes_per_pixel;

                    memcpy(&s_red, img_spng + px_ofs, 2);
                    memcpy(&s_green, img_spng + px_ofs + 2, 2);
                    memcpy(&s_blue, img_spng + px_ofs + 4, 2);

                    memcpy(&p_red, img_png + px_ofs, 2);
                    memcpy(&p_green, img_png + px_ofs + 2, 2);
                    memcpy(&p_blue, img_png + px_ofs + 4, 2);

                    if(have_alpha)
                    {
                        memcpy(&s_alpha, img_spng + px_ofs + 6, 2);
                        memcpy(&p_alpha, img_png + px_ofs + 6, 2);
                        spng_alpha = s_alpha;
                        png_alpha = p_alpha;
                    }

                    spng_red = s_red;
                    spng_green = s_green;
                    spng_blue = s_blue;

                    png_red = p_red;
                    png_green = p_green;
                    png_blue = p_blue;
            }
            else /* FMT_RGBA8 */
            {
                uint8_t s_red, s_green, s_blue, s_alpha;
                uint8_t p_red, p_green, p_blue, p_alpha;

                size_t px_ofs = (x + (y * w)) * bytes_per_pixel;

                memcpy(&s_red, img_spng + px_ofs, 1);
                memcpy(&s_green, img_spng + px_ofs + 1, 1);
                memcpy(&s_blue, img_spng + px_ofs + 2, 1);

                memcpy(&p_red, img_png + px_ofs, 1);
                memcpy(&p_green, img_png + px_ofs + 1, 1);
                memcpy(&p_blue, img_png + px_ofs + 2, 1);

                if(have_alpha)
                {
                    memcpy(&s_alpha, img_spng + px_ofs + 3, 1);
                    memcpy(&p_alpha, img_png + px_ofs + 3, 1);
                    spng_alpha = s_alpha;
                    png_alpha = p_alpha;
                }

                spng_red = s_red;
                spng_green = s_green;
                spng_blue = s_blue;

                png_red = p_red;
                png_green = p_green;
                png_blue = p_blue;

            }

            if(have_alpha && spng_alpha != png_alpha)
            {
                printf("alpha mismatch at x:%u y:%u, spng: %u png: %u\n", x, y, spng_alpha, png_alpha);
                alpha_mismatch = 1;
            }

            if(spng_red != png_red || spng_green != png_green || spng_blue != png_blue)
            {
                printf("color difference at x: %u y:%u, spng: %u %u %u png: %u %u %u\n", x, y,
                       spng_red, spng_green, spng_blue, png_red, png_green, png_blue);
                pixel_diff = 1;
            }
        }
    }

    if(alpha_mismatch || pixel_diff) return 1;

    return 0;
}

int decode_and_compare(unsigned char *pngbuf, size_t siz_pngbuf, int fmt, int flags)
{
    struct spng_ihdr ihdr;
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

    if(!info_printed)
    {
        printf("image info: %ux%u %u bits per sample, type %u, %s\n",
            ihdr.width, ihdr.height, ihdr.bit_depth, ihdr.colour_type,
            ihdr.interlace_method ? "interlaced" : "non-interlaced");
        info_printed = 1;
    }

    int ret_memcmp = memcmp(img_spng, img_png, img_spng_size);

    int ret = compare_images(&ihdr, fmt, img_spng, img_png);

    /* in case compare_images() has some edge case */
    if(!ret && ret_memcmp)
    {
        printf("compare_images() returned 0 but images are not identical\n");
        ret = 1;
    }
    else if(ret && !ret_memcmp)
    {
        printf("compare_images() returned non-zero but images are identical\n");
    }

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
    int e = 0;

    ret = decode_and_compare(pngbuf, siz_pngbuf, SPNG_FMT_RGBA8, 0);
    printf("decode and compare FMT_RGBA8: %s\n", ret ? "fail" : "ok");

    if(ret) e = ret;

    ret = decode_and_compare(pngbuf, siz_pngbuf, SPNG_FMT_RGBA16, 0);
    printf("decode and compare FMT_RGBA16: %s\n", ret ? "fail" : "ok");

    if(!e && ret) e = ret;

    free(pngbuf);

    return e;
}
