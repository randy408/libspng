#include <inttypes.h>

#include "test_spng.h"
#include "test_png.h"

#define SPNG_TEST_COMPARE_CHUNKS 1

struct spng_test_case
{
    int fmt;
    int flags;
    int test_flags;
};

void print_test_args(struct spng_test_case *test_case)
{
    printf("Decode and compare ");
    if(test_case->fmt == SPNG_FMT_RGBA8) printf("RGBA8, ");
    else if(test_case->fmt == SPNG_FMT_RGBA16) printf("RGBA16, ");
    else if(test_case->fmt == SPNG_FMT_RGB8) printf("RGB8, ");
    else if(test_case->fmt == SPNG_FMT_PNG) printf("PNG, ");
    else if(test_case->fmt == SPNG_FMT_RAW) printf("RAW, ");
    else if(test_case->fmt == SPNG_FMT_UNPACK) printf("UNPACK, ");

    printf("FLAGS: ");

    if(!test_case->flags && !test_case->test_flags) printf("(NONE)");

    if(test_case->flags & SPNG_DECODE_TRNS) printf("TRNS ");
    if(test_case->flags & SPNG_DECODE_GAMMA) printf("GAMMA ");

    if(test_case->test_flags & SPNG_TEST_COMPARE_CHUNKS) printf("COMPARE_CHUNKS ");

    printf("\n");
}


void gen_test_cases(struct spng_test_case *test_cases, int *test_cases_n)
{
/*  With libpng it's not possible to request 8/16-bit images regardless of
    PNG format without calling functions that alias to png_set_expand(_16),
    which acts as if png_set_tRNS_to_alpha() was called, as a result
    there are no tests where transparency is not applied
*/

    int n=0;

    test_cases[n].fmt = SPNG_FMT_RGBA8;
    test_cases[n].flags = SPNG_DECODE_TRNS;
    test_cases[n++].test_flags = 0;

    test_cases[n].fmt = SPNG_FMT_RGBA8;
    test_cases[n].flags = SPNG_DECODE_TRNS | SPNG_DECODE_GAMMA;
    test_cases[n++].test_flags = 0;

    test_cases[n].fmt = SPNG_FMT_RGBA16;
    test_cases[n].flags = SPNG_DECODE_TRNS;
    test_cases[n++].test_flags = 0;

    test_cases[n].fmt = SPNG_FMT_RGBA16;
    test_cases[n].flags = SPNG_DECODE_TRNS | SPNG_DECODE_GAMMA;
    test_cases[n++].test_flags = 0;

    test_cases[n].fmt = SPNG_FMT_RGB8;
    test_cases[n].flags = 0;
    test_cases[n++].test_flags = 0;

    test_cases[n].fmt = SPNG_FMT_RGB8;
    test_cases[n].flags = SPNG_DECODE_GAMMA;
    test_cases[n++].test_flags = 0;

    test_cases[n].fmt = SPNG_FMT_PNG;
    test_cases[n].flags = 0;
    test_cases[n++].test_flags = 0;

    test_cases[n].fmt = SPNG_FMT_RAW;
    test_cases[n].flags = 0;
    test_cases[n++].test_flags = 0;

    test_cases[n].fmt = SPNG_FMT_UNPACK;
    test_cases[n].flags = 0;
    test_cases[n++].test_flags = 0;

    *test_cases_n = n;
}

#define SPNG_FMT_RGB16 8

int compare_images(struct spng_ihdr *ihdr, int fmt, int flags, unsigned char *img_spng, unsigned char *img_png)
{
    uint32_t w, h;
    uint32_t x, y;
    uint32_t max_diff=0, diff_div = 50; /* allow up to ~2% difference for each channel */
    uint8_t have_alpha = 1;
    uint8_t alpha_mismatch = 0;
    uint8_t pixel_diff = 0;
    uint8_t bytes_per_pixel = 4; /* SPNG_FMT_RGBA8 */
    const uint8_t samples_per_byte = 8 / ihdr->bit_depth;
    const uint8_t mask = (uint16_t)(1 << ihdr->bit_depth) - 1;
    const uint8_t initial_shift = 8 - ihdr->bit_depth;
    uint8_t shift_amount = initial_shift;
    size_t row_width, px_ofs;
    unsigned channels;

    uint32_t red_diff = 0, green_diff = 0, blue_diff = 0, sample_diff = 0;
    uint16_t spng_red = 0, spng_green = 0, spng_blue = 0, spng_alpha = 0, spng_sample = 0;
    uint16_t png_red = 0, png_green = 0, png_blue = 0, png_alpha = 0, png_sample = 0;

    w = ihdr->width;
    h = ihdr->height;

    if(fmt == SPNG_FMT_PNG)
    {
        if(ihdr->color_type == SPNG_COLOR_TYPE_TRUECOLOR)
        {
            if(ihdr->bit_depth == 8) fmt = SPNG_FMT_RGB8;
            else fmt = SPNG_FMT_RGB16;
        }
        else if(ihdr->color_type == SPNG_COLOR_TYPE_TRUECOLOR_ALPHA)
        {
            if(ihdr->bit_depth == 8) fmt = SPNG_FMT_RGBA8;
            else fmt = SPNG_FMT_RGBA16;
        }
        else
        {/* gray 1,2,4,8,16 bits, indexed 1,2,4,8 or gray alpha 8,16  */
            channels = 1; /* grayscale or indexed color */
            if(ihdr->color_type == SPNG_COLOR_TYPE_GRAYSCALE_ALPHA) channels = 2;
            else have_alpha = 0;

            if(ihdr->bit_depth < 8) bytes_per_pixel = 1;
            else bytes_per_pixel = channels * (ihdr->bit_depth / 8);

            row_width = (channels * ihdr->bit_depth * w + 7) / 8;

            if(ihdr->color_type == SPNG_COLOR_TYPE_INDEXED) flags &= ~SPNG_DECODE_GAMMA;
        }
    }

    if(fmt == SPNG_FMT_RGBA8)
    {
        bytes_per_pixel = 4;
        max_diff = 256 / diff_div;
    }
    else if(fmt == SPNG_FMT_RGBA16)
    {
        bytes_per_pixel = 8;
        max_diff = 65536 / diff_div;
    }
    else if(fmt == SPNG_FMT_RGB8)
    {
        bytes_per_pixel = 3;
        have_alpha = 0;
        max_diff = 256 / diff_div;
    }
    else if(fmt == SPNG_FMT_RGB16)
    {
        bytes_per_pixel = 6;
        have_alpha = 0;
        max_diff = 65536 / diff_div;
    }

    for(y=0; y < h; y++)
    {
        for(x=0; x < w; x++)
        {
            if(fmt == SPNG_FMT_PNG && ihdr->bit_depth < 8)
                px_ofs = (y * row_width) + x / samples_per_byte;
            else
                px_ofs = (x + (y * w)) * bytes_per_pixel;

            if(fmt & (SPNG_FMT_RGBA16 | SPNG_FMT_RGB16))
            {
                uint16_t s_red, s_green, s_blue, s_alpha;
                uint16_t p_red, p_green, p_blue, p_alpha;

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
            else if(fmt & (SPNG_FMT_RGBA8 | SPNG_FMT_RGB8))
            {
                uint8_t s_red, s_green, s_blue, s_alpha;
                uint8_t p_red, p_green, p_blue, p_alpha;

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
            else if(fmt == SPNG_FMT_PNG)
            {
                if(ihdr->bit_depth <= 8) /* gray 1-8, gray-alpha 8, indexed 1-8 */
                {
                    uint8_t s_alpha, s_sample;
                    uint8_t p_alpha, p_sample;

                    memcpy(&s_sample, img_spng + px_ofs, 1);
                    memcpy(&p_sample, img_png + px_ofs, 1);

                    if(shift_amount > 7) shift_amount = initial_shift;

                    s_sample = (s_sample >> shift_amount) & mask;
                    p_sample = (p_sample >> shift_amount) & mask;

                    shift_amount -= ihdr->bit_depth;

                    spng_sample = s_sample;
                    png_sample = p_sample;

                   if(have_alpha)
                    {
                        memcpy(&s_alpha, img_spng + px_ofs + 1, 1);
                        memcpy(&p_alpha, img_png + px_ofs + 1, 1);
                        spng_alpha = s_alpha;
                        png_alpha = p_alpha;
                    }
                }
                else /* gray 16, gray-alpha 16 */
                {
                    uint16_t s_alpha, s_sample;
                    uint16_t p_alpha, p_sample;

                    memcpy(&s_sample, img_spng + px_ofs, 2);
                    memcpy(&p_sample, img_png + px_ofs, 2);

                    spng_sample = s_sample;
                    png_sample = p_sample;

                    if(have_alpha)
                    {
                        memcpy(&s_alpha, img_spng + px_ofs + 2, 2);
                        memcpy(&p_alpha, img_png + px_ofs + 2, 2);
                        spng_alpha = s_alpha;
                        png_alpha = p_alpha;
                    }
                }
            }

            if(spng_red != png_red || spng_green != png_green || spng_blue != png_blue || spng_sample != png_sample)
            {
                if(flags & SPNG_DECODE_GAMMA)
                {
                    red_diff = abs(spng_red - png_red);
                    green_diff = abs(spng_green - png_green);
                    blue_diff = abs(spng_blue - png_blue);
                    sample_diff = abs(spng_sample - png_sample);

                    if(red_diff > max_diff || green_diff > max_diff || blue_diff > max_diff)
                    {
                        printf("invalid gamma correction at x: %" PRIu32 " y: %" PRIu32 ", "
                               "spng: %" PRIu16 " %" PRIu16 " %" PRIu16 " "
                               "png: %" PRIu16 " %" PRIu16 " %" PRIu16 "\n",
                               x, y,
                               spng_red, spng_green, spng_blue,
                               png_red, png_green, png_blue);
                        pixel_diff = 1;
                    }
                    else if(sample_diff > max_diff)
                    {
                        printf("invalid gamma correction at x: %" PRIu32 " y: %" PRIu32 ", "
                               "spng: %" PRIu16 " png: %" PRIu16 "\n", x, y, spng_sample, png_sample);
                        pixel_diff = 1;
                    }
                }
                else
                {
                    if(spng_sample != png_sample)
                    {
                        char *issue_str = "";
                        if(ihdr->color_type == SPNG_COLOR_TYPE_INDEXED) issue_str = "index mismatch";
                        else issue_str = "grayscale difference";

                        printf("%s at x: %u y: %u spng: %u png: %u\n",
                            issue_str, x, y, spng_sample, png_sample);
                    }
                    else
                    {
                        printf("color difference at x: %" PRIu32 " y: %" PRIu32 ", "
                               "spng: %" PRIu16 " %" PRIu16 " %" PRIu16 " "
                               "png: %" PRIu16 " %" PRIu16 " %" PRIu16 "\n",
                               x, y,
                               spng_red, spng_green, spng_blue,
                               png_red, png_green, png_blue);
                    }

                    pixel_diff = 1;
                }
            }

            if(have_alpha && spng_alpha != png_alpha)
            {
                printf("alpha mismatch at x:%" PRIu32 " y:%" PRIu32 ", "
                       "spng: %" PRIu16 " png: %" PRIu16 "\n",
                       x, y, spng_alpha, png_alpha);
                alpha_mismatch = 1;
            }
        }
    }

    if(alpha_mismatch || pixel_diff) return 1;

    return 0;
}

int decode_and_compare(FILE *file, int fmt, int flags)
{
    struct spng_ihdr ihdr;
    size_t img_spng_size;
    unsigned char *img_spng =  NULL;

    img_spng = getimage_libspng(file, &img_spng_size, fmt, flags, &ihdr);
    if(img_spng == NULL)
    {
        printf("getimage_libspng() failed\n");
        return 1;
    }

    rewind(file);

    size_t img_png_size;
    unsigned char *img_png = NULL;

    img_png = getimage_libpng(file, &img_png_size, fmt, flags);
    if(img_png == NULL)
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

    if(!memcmp(img_spng, img_png, img_spng_size)) goto identical;

    ret = compare_images(&ihdr, fmt, flags, img_spng, img_png);

    if(!ret && !(flags & SPNG_DECODE_GAMMA))
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

    FILE *file;
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

    file = fopen(filename, "rb");

    if(file == NULL)
    {
        printf("error opening input file %s\n", filename);
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

        int e = decode_and_compare(file, test_cases[i].fmt, test_cases[i].flags);
        if(!ret) ret = e;
        rewind(file);
    }

    return ret;
}
