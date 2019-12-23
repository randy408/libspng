/*#define TEST_SPNG_STREAM_READ_INFO*/

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

int should_fail = 0;
int info_printed = 0;


void print_test_args(struct spng_test_case *test_case)
{
    printf("Decode and compare ");
    if(test_case->fmt == SPNG_FMT_RAW) printf("RAW, ");
    else if(test_case->fmt == SPNG_FMT_RGBA8) printf("RGBA8, ");
    else if(test_case->fmt == SPNG_FMT_RGBA16) printf("RGBA16, ");

    printf("FLAGS: ");

    if(!test_case->flags && !test_case->test_flags) printf("(NONE)");

    if(test_case->flags & SPNG_DECODE_TRNS) printf("TRNS ");
    if(test_case->flags & SPNG_DECODE_GAMMA) printf("GAMMA ");

    if(test_case->test_flags & SPNG_TEST_COMPARE_CHUNKS) printf("COMPARE_CHUNKS ");

    printf("\n");
}


void gen_test_cases(struct spng_test_case *test_cases, int *test_cases_n)
{
/* With libpng it's not possible to request 8/16-bit images regardless of
       PNG format without calling functions that alias to png_set_expand(_16),
       which acts as if png_set_tRNS_to_alpha() was called, as a result
       there are no tests where transparency is not applied */
    #define NCASES 5
    int i, fmts_and_flags[2*NCASES] = {
        SPNG_FMT_RAW, 0,
        SPNG_FMT_RGBA8, SPNG_DECODE_TRNS,
        SPNG_FMT_RGBA8, SPNG_DECODE_TRNS | SPNG_DECODE_GAMMA,
        SPNG_FMT_RGBA16, SPNG_DECODE_TRNS,
        SPNG_FMT_RGBA16, SPNG_DECODE_TRNS | SPNG_DECODE_GAMMA

    };
    *test_cases_n = NCASES;
    for (i = 0; i < NCASES; i++) {
        test_cases[i].fmt = fmts_and_flags[2*i];
        test_cases[i].flags = fmts_and_flags[2*i + 1];
    }
}


int compare_images(struct spng_ihdr *ihdr, int fmt, int flags, unsigned char *img_spng, unsigned char *img_png)
{
    uint32_t w, h;
    uint32_t x, y;
    uint8_t have_alpha = 1;
    uint8_t alpha_mismatch = 0;
    uint8_t pixel_diff = 0;
    uint8_t bytes_per_pixel = 0;
    size_t bits_per_sample, nsamples, row_bytes, extra_bits; /* Used with SPNG_FMT_RAW */

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
    else
    {
        switch ((enum spng_color_type)ihdr->color_type) {
            case SPNG_COLOR_TYPE_GRAYSCALE:
            case SPNG_COLOR_TYPE_INDEXED:
                nsamples = 1;
                break;
            case SPNG_COLOR_TYPE_TRUECOLOR:
                nsamples = 3;
                break;
            case SPNG_COLOR_TYPE_GRAYSCALE_ALPHA:
                nsamples = 2;
                break;
            case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA:
                nsamples = 4;
                break;
        }
        bits_per_sample = ihdr->bit_depth;

        /* total pixel size of 1 byte or greater, allowed PNG formats
        will never have a pixel size of 1.5 bytes, 2.5 bytes, etc.  */
        if (nsamples * bits_per_sample / 8)
        {
            bytes_per_pixel = nsamples * bits_per_sample / 8;
            row_bytes = ihdr->width * bytes_per_pixel;
            extra_bits = 0;
        }
        else
        {
            size_t row_bits = bits_per_sample * nsamples * ihdr->width;
            row_bytes = row_bits / 8;
            extra_bits = row_bits % 8;
        }
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
            else if(fmt == SPNG_FMT_RGBA8)
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
            else /* SPNG_FMT_RAW */
            {
                /* calculate the offset in bytes and then an additional
                bit offset if necessary*/
                size_t offset_bytes, offset_bits;
                offset_bytes = row_bytes*x + (extra_bits * y)/8;
                offset_bits = (extra_bits * y) % 8;

                /* Given the allowed bit depths, we know a sample's bits
                will never span a byte boundary, for 16bit values we set
                the local bits per sample (bps) to 8 so no shifting occurs
                since the offset_bits should also be 0 */
                unsigned char s_samples[8] = {0,0,0,0,0,0,0,0};
                unsigned char p_samples[8] = {0,0,0,0,0,0,0,0};
                unsigned char *s_ptr=img_spng+offset_bytes, *p_ptr=img_png+offset_bytes;
                unsigned char s_byte, p_byte;
                size_t bps = bits_per_sample == 16 ? 8 : bits_per_sample;
                size_t l_shift, r_shift = 8 - bps;
                for (int i = 0; i < nsamples; i++) {
                    s_byte=*s_ptr;
                    p_byte=*p_ptr;
                    l_shift = offset_bits;
                    s_samples[i] = (s_byte << l_shift) >> r_shift;
                    p_samples[i] = (p_byte << l_shift) >> r_shift;
                    offset_bits += bps;
                    if (offset_bits == 8) {
                        offset_bits = 0;
                        s_ptr++;
                        p_ptr++;
                    }
                }

                /* convert samples array into pixels to compare */
                bps = bits_per_sample == 16 ? 2 : 1;
                spng_alpha = png_alpha = (1 << bits_per_sample) - 1;
                if (nsamples == 1) /* grayscale */
                {
                    memcpy(&spng_red, s_samples, bps);
                    memcpy(&png_red, p_samples, bps);
                    spng_green = spng_blue = spng_red;
                    png_green = png_blue = png_red;
                }
                else if (nsamples == 2) /* grayscale + alpha */
                {
                    memcpy(&spng_red, s_samples, bps);
                    memcpy(&png_red, p_samples, bps);
                    memcpy(&spng_alpha, s_samples + bps, bps);
                    memcpy(&png_alpha, p_samples + bps, bps);
                    spng_green = spng_blue = spng_red;
                    png_green = png_blue = png_red;
                }
                else
                {
                    memcpy(&spng_red, s_samples, bps);
                    memcpy(&png_red, p_samples, bps);
                    memcpy(&spng_green, s_samples+bps, bps);
                    memcpy(&png_green, p_samples+bps, bps);
                    memcpy(&spng_blue, s_samples + 2*bps, bps);
                    memcpy(&png_blue, p_samples + 2*bps, bps);
                    if (nsamples == 4)
                    {
                        memcpy(&spng_alpha, s_samples + 3*bps, bps);
                        memcpy(&png_alpha, p_samples + 3*bps, bps);
                    }
                }
            }

            if(have_alpha && spng_alpha != png_alpha)
            {
                printf("alpha mismatch at x:%" PRIu32 " y:%" PRIu32 ", "
                       "spng: %" PRIu16 " png: %" PRIu16 "\n",
                       x, y, spng_alpha, png_alpha);
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
