/*#define TEST_SPNG_STREAM_READ_INFO*/

#include <inttypes.h>

#include "test_spng.h"
#include "test_png.h"

struct spng_test_case
{
    int fmt;
    int flags;
};

int should_fail = 0;
int info_printed = 0;


void print_test_args(struct spng_test_case *test_case)
{
    printf("Decode and compare ");
    if(test_case->fmt == SPNG_FMT_RGBA8) printf("RGBA8, ");
    else if(test_case->fmt == SPNG_FMT_RGBA16) printf("RGBA16, ");

    printf("FLAGS: ");

    if(!test_case->flags) printf("(NONE)");
    if(test_case->flags & SPNG_DECODE_TRNS) printf("TRNS ");
    if(test_case->flags & SPNG_DECODE_GAMMA) printf("GAMMA ");

    printf("\n");
}

void gen_test_cases(struct spng_test_case *test_cases, int *test_cases_n)
{
    const int fmt_list[] = { SPNG_FMT_RGBA8, SPNG_FMT_RGBA16 };
    const int fmt_n = 2;

    int i, j;
    int k = 0;

    for(i=0; i < fmt_n; i++)
    {
    /* With libpng it's not possible to request 8/16-bit images regardless of
       PNG format without calling functions that alias to png_set_expand(_16),
       which acts as if png_set_tRNS_to_alpha() was called, as a result
       there are no tests where the tRNS chunk is ignored. */
        for(j=1; j <= SPNG_DECODE_GAMMA; j++, k++)
        {
            test_cases[k].fmt = fmt_list[i];
            test_cases[k].flags = j | SPNG_DECODE_TRNS;
        }
    }

    *test_cases_n = k;
}

int compare_images(struct spng_ihdr *ihdr, int fmt, int flags, unsigned char *img_spng, unsigned char *img_png)
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
                printf("alpha mismatch at x:%" PRIu32 " y:%" PRIu32 ", "
                       "spng: %" PRIu16 " png: %" PRIu16 "\n",
                       x, y,
                       spng_alpha, png_alpha);
                alpha_mismatch = 1;
            }

            if(spng_red != png_red || spng_green != png_green || spng_blue != png_blue)
            {
                if(flags & SPNG_DECODE_GAMMA)
                {
                    uint32_t red_diff, green_diff, blue_diff;
                    uint32_t max_diff=0;

                    /* allow up to ~2% difference for each channel */
                    if(fmt == SPNG_FMT_RGBA8) max_diff = 256 / 50;
                    else if(fmt == SPNG_FMT_RGBA16) max_diff = 65536 / 50;

                    red_diff = abs(spng_red - png_red);
                    green_diff = abs(spng_green - png_green);
                    blue_diff = abs(spng_blue - png_blue);

                    if(red_diff > max_diff || green_diff > max_diff || blue_diff > max_diff)
                    {
                        printf("invalid gamma correction at x: %" PRIu32 " y:%" PRIu32 ", "
                               "spng: %" PRIu16 " %" PRIu16 " %" PRIu16 " "
                               "png: %" PRIu16 " %" PRIu16 " %" PRIu16 "\n",
                               x, y,
                               spng_red, spng_green, spng_blue,
                               png_red, png_green, png_blue);
                        pixel_diff = 1;
                    }
                }
                else
                {
                    printf("color difference at x: %" PRIu32 " y:%" PRIu32 ", "
                           "spng: %" PRIu16 " %" PRIu16 " %" PRIu16 " "
                           "png: %" PRIu16 " %" PRIu16 " %" PRIu16 "\n",
                           x, y,
                           spng_red, spng_green, spng_blue,
                           png_red, png_green, png_blue);
                    pixel_diff = 1;
                }
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

    int ret = 0;
    int ret_memcmp = memcmp(img_spng, img_png, img_spng_size);

    if(!ret_memcmp) goto identical;

    ret = compare_images(&ihdr, fmt, flags, img_spng, img_png);

    if( !(flags & SPNG_DECODE_GAMMA) && !ret)
    {/* in case compare_images() has some edge case */
        printf("compare_images() returned 0 but images are not identical\n");
        ret = 1;
    }

identical:

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

    if(!strcmp(filename, "info"))
    {
        unsigned int png_ver = png_access_version_number();

        printf("spng header version: %u.%u.%u, library version: %s\n",
               SPNG_VERSION_MAJOR, SPNG_VERSION_MINOR, SPNG_VERSION_PATCH,
               spng_version_string());
        printf("png header version: %u.%u.%u, library version: %u.%u.%u\n",
               PNG_LIBPNG_VER_MAJOR, PNG_LIBPNG_VER_MINOR, PNG_LIBPNG_VER_RELEASE,
               png_ver / 10000, png_ver / 100 % 100, png_ver % 100);


        return 0;
    }

    png = fopen(filename, "rb");

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

    struct spng_test_case test_cases[100];
    int test_cases_n;

    gen_test_cases(test_cases, &test_cases_n);

    int ret=0;
    uint32_t i;
    for(i=0; i < test_cases_n; i++)
    {
        print_test_args(&test_cases[i]);

        int e = decode_and_compare(pngbuf, siz_pngbuf, test_cases[i].fmt, test_cases[i].flags);
        if(!ret) ret = e;
    }

    free(pngbuf);

    return ret;
}
