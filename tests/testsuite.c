#include <inttypes.h>

#include "test_spng.h"
#include "test_png.h"

enum spngt_flags
{
    SPNGT_COMPARE_CHUNKS = 1
};

#define SPNG_FMT_RGB16 8

struct spng_test_case
{
    int fmt;
    int flags;
    int test_flags;
};

static int n_test_cases=0;
struct spng_test_case test_cases[100];

const char* fmt_str(int fmt)
{
    switch(fmt)
    {
        case SPNG_FMT_RGBA8: return "RGBA8";
        case SPNG_FMT_RGBA16: return "RGBA16";
        case SPNG_FMT_RGB8: return "RGB8";
        case SPNG_FMT_GA8: return "GA8";
        case SPNG_FMT_GA16: return "GA16";
        case SPNG_FMT_G8: return "G8";
        case SPNG_FMT_PNG: return "PNG";
        case SPNG_FMT_RAW: return "RAW";
        case SPNGT_FMT_VIPS: return "VIPS";
        default: return "   ";
    }
}

static void print_test_args(struct spng_test_case *test_case)
{
    printf("Decode and compare %s", fmt_str(test_case->fmt));

    char pad_str[] = "      ";
    pad_str[sizeof(pad_str) - strlen(fmt_str(test_case->fmt))] = '\0';

    printf(",%sFLAGS: ", pad_str);

    if(!test_case->flags && !test_case->test_flags) printf("(NONE)");

    if(test_case->flags & SPNG_DECODE_TRNS) printf("TRNS ");
    if(test_case->flags & SPNG_DECODE_GAMMA) printf("GAMMA ");

    if(test_case->test_flags & SPNGT_COMPARE_CHUNKS) printf("COMPARE_CHUNKS ");

    printf("\n");

    fflush(stdout);
}

static void add_test_case(int fmt, int flags, int test_flags)
{
    int n = n_test_cases;

    test_cases[n].fmt = fmt;
    test_cases[n].flags = flags;
    test_cases[n_test_cases++].test_flags = test_flags;
}

/* Returns 1 on different images with, allows a small difference if gamma corrected */
static int compare_images(const struct spng_ihdr *ihdr,
                          int fmt,
                          int flags,
                          const unsigned char *img_spng,
                          const unsigned char *img_png,
                          size_t img_size)
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
    size_t row_width = img_size / ihdr->height;
    size_t px_ofs = 0;
    unsigned channels;

    uint32_t red_diff = 0, green_diff = 0, blue_diff = 0, sample_diff = 0;
    uint16_t spng_red = 0, spng_green = 0, spng_blue = 0, spng_alpha = 0, spng_sample = 0;
    uint16_t png_red = 0, png_green = 0, png_blue = 0, png_alpha = 0, png_sample = 0;

    w = ihdr->width;
    h = ihdr->height;

    if(fmt & (SPNG_FMT_PNG | SPNG_FMT_RAW))
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
    else if(fmt == SPNG_FMT_GA8)
    {
        bytes_per_pixel = 2;
        have_alpha = 1;
        max_diff = 256 / diff_div;
    }
    else if(fmt == SPNG_FMT_G8)
    {
        bytes_per_pixel = 1;
        have_alpha = 0;
        max_diff = 256 / diff_div;
    }

    for(y=0; y < h; y++)
    {
        for(x=0; x < w; x++)
        {
            if(fmt & (SPNG_FMT_PNG | SPNG_FMT_RAW) && ihdr->bit_depth < 8)
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
            else if(fmt & (SPNG_FMT_PNG | SPNG_FMT_RAW | SPNG_FMT_G8 | SPNG_FMT_GA8 | SPNG_FMT_GA16))
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

static void print_chunks(spngt_chunk_bitfield chunks)
{
    spngt_chunk_bitfield none = { 0 };

    if(!memcmp(&none, &chunks, sizeof(spngt_chunk_bitfield))) printf(" (none)");

    if(chunks.plte) printf(" PLTE");
    if(chunks.trns) printf(" tRNS");
    if(chunks.chrm) printf(" cHRM");
    if(chunks.gama) printf(" gAMA");
    if(chunks.iccp) printf(" iCCP");
    if(chunks.sbit) printf(" sBIT");
    if(chunks.srgb) printf(" sRGB");
    if(chunks.text) printf(" tEXt");
    if(chunks.ztxt) printf(" zTXt");
    if(chunks.itxt) printf(" iTXt");
    if(chunks.bkgd) printf(" bKGD");
    if(chunks.hist) printf(" hIST");
    if(chunks.phys) printf(" pHYs");
    if(chunks.splt) printf(" sPLT");
    if(chunks.time) printf(" tIME");
    if(chunks.offs) printf(" oFFs");
    if(chunks.exif) printf(" eXIF");

}

static int compare_chunks(spng_ctx *ctx, png_infop info_ptr, png_structp png_ptr, int after_idat)
{
    spngt_chunk_bitfield spng_have = { 0 };
    spngt_chunk_bitfield png_have = { 0 };

    int i, ret = 0;
    struct spng_plte plte;
    struct spng_trns trns;
    struct spng_chrm chrm;
    double gamma;
    struct spng_iccp iccp;
    struct spng_sbit sbit;
    uint8_t srgb_rendering_intent;
    struct spng_text *text = NULL;
    struct spng_bkgd bkgd;
    struct spng_hist hist;
    struct spng_phys phys;
    struct spng_splt *splt = NULL;
    struct spng_time time;
    uint32_t n_text = 0, n_splt = 0;
    struct spng_offs offs;
    struct spng_exif exif;

    if(!spng_get_plte(ctx, &plte)) spng_have.plte = 1;
    if(!spng_get_trns(ctx, &trns)) spng_have.trns = 1;
    if(!spng_get_chrm(ctx, &chrm)) spng_have.chrm = 1;
    if(!spng_get_gama(ctx, &gamma)) spng_have.gama = 1;
    if(!spng_get_iccp(ctx, &iccp)) spng_have.iccp = 1;
    if(!spng_get_sbit(ctx, &sbit)) spng_have.sbit = 1;
    if(!spng_get_srgb(ctx, &srgb_rendering_intent)) spng_have.srgb = 1;
    if(!spng_get_text(ctx, NULL, &n_text)) spng_have.text = 1;
    if(!spng_get_bkgd(ctx, &bkgd)) spng_have.bkgd = 1;
    if(!spng_get_hist(ctx, &hist)) spng_have.hist = 1;
    if(!spng_get_phys(ctx, &phys)) spng_have.phys = 1;
    if(!spng_get_splt(ctx, NULL, &n_splt)) spng_have.splt = 1;
    if(!spng_get_time(ctx, &time)) spng_have.time = 1;
    if(!spng_get_offs(ctx, &offs)) spng_have.offs = 1;
    if(!spng_get_exif(ctx, &exif)) spng_have.exif = 1;

    png_text *png_text;
    int png_n_text;
    png_sPLT_t *png_splt_entries;

    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_PLTE)) png_have.plte = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) png_have.trns = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_cHRM)) png_have.chrm = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_gAMA)) png_have.gama = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_iCCP)) png_have.iccp = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_sBIT)) png_have.sbit = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_sRGB)) png_have.srgb = 1;
    if(png_get_text(png_ptr, info_ptr, &png_text, &png_n_text)) png_have.text = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_bKGD)) png_have.bkgd = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_hIST)) png_have.hist = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_pHYs)) png_have.phys = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_sPLT)) png_have.splt = 1;
    if(png_get_sPLT(png_ptr, info_ptr, &png_splt_entries)) png_have.splt = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tIME)) png_have.time = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_oFFs)) png_have.offs = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_eXIf)) png_have.exif = 1;

    const char *pos = after_idat ? " after IDAT" : "before IDAT";

    printf("[%s] spng chunks:  ", pos);
    print_chunks(spng_have);
    printf("\n");

    printf("[%s] libpng chunks:", pos);
    print_chunks(png_have);
    printf("\n");

    if(memcmp(&spng_have, &png_have, sizeof(spngt_chunk_bitfield)))
    {
        printf("[%s] ERROR: metadata mismatch!\n", pos);
        return 1;
    }

    if(n_text != png_n_text)
    {
        printf("text chunk count mismatch: %u(spng), %d(libpng)\n", n_text, png_n_text);
        return 1;
    }

    if(n_text)
    {
        text = malloc(n_text * sizeof(struct spng_text));
        if(!text) return 1;

        spng_get_text(ctx, text, &n_text);

        for(i=0; i < n_text; i++)
        {
            if((strcmp(png_text[i].text, text[i].text)))
            {
                printf("text[%d]: text mismatch!\nspng: %s\n\nlibpng: %s\n", i, text[i].text, png_text[i].text);
                ret = 1;
            }
        }
    }

    uint32_t n_spng_chunks = 0;
    struct spng_unknown_chunk *spng_chunks = NULL;

    int n_png_chunks;
    png_unknown_chunkp png_chunks;

    if(!spng_get_unknown_chunks(ctx, NULL, &n_spng_chunks))
    {
        spng_chunks = malloc(n_spng_chunks * sizeof(struct spng_unknown_chunk));
        spng_get_unknown_chunks(ctx, spng_chunks, &n_spng_chunks);
    }

    n_png_chunks = png_get_unknown_chunks(png_ptr, info_ptr, &png_chunks);

    if(n_png_chunks != n_spng_chunks)
    {
        printf("unknown chunk count mismatch: %u(spng), %d(libpng)\n", n_spng_chunks, n_png_chunks);
        ret = 1;
        goto cleanup;
    }

    for(i=0; i < n_spng_chunks; i++)
    {
        if(spng_chunks[i].length != png_chunks[i].size)
        {
            printf("chunk[%d]: size mismatch %" PRIu64 "(spng) %" PRIu64" (libpng)\n", i, spng_chunks[i].length, png_chunks[i].size);
            ret = 1;
        }
    }

cleanup:
    free(splt);
    free(text);
    free(spng_chunks);

    return ret;
}

static int decode_and_compare(const char *filename, int fmt, int flags, int test_flags)
{
    int ret = 0;

    spng_ctx *ctx = NULL;
    FILE *file_spng = NULL;
    size_t img_spng_size;
    unsigned char *img_spng =  NULL;

    png_infop info_ptr = NULL;
    png_structp png_ptr = NULL;
    FILE *file_libpng = NULL;
    size_t img_png_size;
    unsigned char *img_png = NULL;

    file_spng = fopen(filename, "rb");
    file_libpng = fopen(filename, "rb");

    if(!file_spng || !file_libpng)
    {
        ret = 1;
        goto cleanup;
    }

    struct spng_ihdr ihdr;
    ctx = init_spng(file_spng, 0, &ihdr);

    if(ctx == NULL)
    {
        ret = 1;
        goto cleanup;
    }

    img_spng = getimage_spng(ctx, &img_spng_size, fmt, flags);
    if(img_spng == NULL)
    {
        printf("getimage_spng() failed\n");
        ret = 1;
        goto cleanup;
    }

    png_ptr = init_libpng(file_libpng, 0, &info_ptr);
    if(png_ptr == NULL)
    {
        ret = 1;
        goto cleanup;
    }

    if(test_flags & SPNGT_COMPARE_CHUNKS)
    {
        ret = compare_chunks(ctx, info_ptr, png_ptr, 0);
    }

    img_png = getimage_libpng(png_ptr, info_ptr, &img_png_size, fmt, flags);
    if(img_png == NULL)
    {
        printf("getimage_libpng() failed\n");
        ret = 1;
        goto cleanup;
    }

    if(img_png_size != img_spng_size)
    {
        printf("output image size mismatch\n");
        printf("spng: %lu\n png: %lu\n", (unsigned long int)img_spng_size, (unsigned long int)img_png_size);
        ret = 1;
        goto cleanup;
    }

    if(fmt == SPNGT_FMT_VIPS)
    {/* Get the right format for compare_images() */
        fmt = SPNG_FMT_PNG;
        if(ihdr.color_type == SPNG_COLOR_TYPE_INDEXED) fmt = SPNG_FMT_RGB8;
        else if(ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE && ihdr.bit_depth < 8) fmt = SPNG_FMT_G8;

        spng_get_trns_fmt(ctx, &fmt);
        printf("VIPS format: %s\n", fmt_str(fmt));
    }

    if(!memcmp(img_spng, img_png, img_spng_size)) goto identical;

    if( !(flags & SPNG_DECODE_GAMMA) )
    {
        printf("error: image buffers are not identical\n");
        ret = 1;
    }

    ret |= compare_images(&ihdr, fmt, flags, img_spng, img_png, img_spng_size);

    if(!ret && !(flags & SPNG_DECODE_GAMMA))
    {/* in case compare_images() has some edge case */
        printf("compare_images() returned 0 but images are not identical\n");
        ret = 1;
        goto cleanup;
    }

identical:

    if(test_flags & SPNGT_COMPARE_CHUNKS)
    {
        ret = compare_chunks(ctx, info_ptr, png_ptr, 1);
    }

cleanup:

    spng_ctx_free(ctx);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    free(img_spng);
    free(img_png);

    if(file_spng) fclose(file_spng);
    if(file_libpng) fclose(file_libpng);

    return ret;
}

static int get_image_info(FILE *f, struct spng_ihdr *ihdr)
{
    spng_ctx *ctx = spng_ctx_new(0);

    spng_set_crc_action(ctx, SPNG_CRC_USE, SPNG_CRC_USE);

    spng_set_png_file(ctx, f);

    int ret = spng_get_ihdr(ctx, ihdr);

    spng_ctx_free(ctx);

    return ret;
}

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        printf("no input file\n");
        return 1;
    }

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

    FILE *file = fopen(filename, "rb");

    if(file == NULL)
    {
        printf("error opening input file %s\n", filename);
        return 1;
    }

    struct spng_ihdr ihdr = {0};

    if(!get_image_info(file, &ihdr))
    {
         char *clr_type_str;
        if(ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE)
            clr_type_str = "GRAY";
        else if(ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR)
            clr_type_str = "RGB";
        else if(ihdr.color_type == SPNG_COLOR_TYPE_INDEXED)
            clr_type_str = "INDEXED";
        else if(ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE_ALPHA)
            clr_type_str = "GRAY-ALPHA";
        else
            clr_type_str = "RGBA";

        printf("%s %" PRIu8 "-bit, %" PRIu32 "x%" PRIu32 " %s\n",
               clr_type_str, ihdr.bit_depth, ihdr.width, ihdr.height,
               ihdr.interlace_method ? "interlaced" : "non-interlaced");
    }
    else printf("failed to get image info\n");

    fclose(file);

/*  With libpng it's not possible to request 8/16-bit images regardless of
    PNG format without calling functions that alias to png_set_expand(_16),
    which acts as if png_set_tRNS_to_alpha() was called, as a result
    there are no tests where transparency is not applied
*/
    int gamma_bug = 0;

    /* https://github.com/randy408/libspng/issues/17 */
    if(ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE) gamma_bug = 1;

    add_test_case(SPNG_FMT_RGBA8, SPNG_DECODE_TRNS, SPNGT_COMPARE_CHUNKS);
    if(!gamma_bug) add_test_case(SPNG_FMT_RGBA8, SPNG_DECODE_TRNS | SPNG_DECODE_GAMMA, 0);
    add_test_case(SPNG_FMT_RGBA16, SPNG_DECODE_TRNS, 0);
    add_test_case(SPNG_FMT_RGBA16, SPNG_DECODE_TRNS | SPNG_DECODE_GAMMA, 0);
    add_test_case(SPNG_FMT_RGB8, 0, 0);
    if(!gamma_bug) add_test_case(SPNG_FMT_RGB8, SPNG_DECODE_GAMMA, 0);
    add_test_case(SPNG_FMT_PNG, 0, 0);
    add_test_case(SPNG_FMT_RAW, 0, 0);

    if(ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE && ihdr.bit_depth <= 8)
    {
        add_test_case(SPNG_FMT_G8, 0, 0);
        add_test_case(SPNG_FMT_GA8, 0, 0);
        add_test_case(SPNG_FMT_GA8, SPNG_DECODE_TRNS, 0);
    }

    if(ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE && ihdr.bit_depth == 16)
    {
        add_test_case(SPNG_FMT_GA16, 0, 0);
        add_test_case(SPNG_FMT_GA16, SPNG_DECODE_TRNS, 0);
    }

    /* This tests the input->output format logic used in libvips,
       it emulates the behavior of their old PNG loader which used libpng. */
    add_test_case(SPNGT_FMT_VIPS, SPNG_DECODE_TRNS, 0);

    printf("%d test cases\n", n_test_cases);

    int ret = 0;
    int i;
    for(i=0; i < n_test_cases; i++)
    {
        print_test_args(&test_cases[i]);

        int e = decode_and_compare(filename, test_cases[i].fmt, test_cases[i].flags, test_cases[i].test_flags);
        if(!ret) ret = e;
    }

    return ret;
}
