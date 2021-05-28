#include "spngt_common.h"

#include "test_spng.h"
#include "test_png.h"

static int n_test_cases=0;
static struct spngt_test_case test_cases[100];

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

static void print_test_args(struct spngt_test_case *test_case)
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
    struct spngt_chunk_info spng = { 0 };
    struct spngt_chunk_info png = { 0 };

    int i, ret = 0;
    struct spng_ihdr ihdr;
    struct spng_plte plte;
    struct spng_trns trns;
    struct spng_chrm chrm;
    struct spng_chrm_int chrm_int;
    double gamma;
    uint32_t gamma_int;
    struct spng_iccp iccp;
    struct spng_sbit sbit;
    uint8_t srgb_rendering_intent;
    struct spng_text *text = NULL;
    struct spng_bkgd bkgd;
    struct spng_hist hist;
    struct spng_phys phys;
    struct spng_splt *splt = NULL;
    struct spng_time time;
    struct spng_offs offs;
    struct spng_exif exif;

    if(!spng_get_ihdr(ctx, &ihdr)) spng.have.ihdr = 1;
    if(!spng_get_plte(ctx, &plte)) spng.have.plte = 1;
    if(!spng_get_trns(ctx, &trns)) spng.have.trns = 1;
    if(!spng_get_chrm(ctx, &chrm)) spng.have.chrm = 1;
    if(spng.have.chrm) spng_get_chrm_int(ctx, &chrm_int);
    if(!spng_get_gama(ctx, &gamma)) spng.have.gama = 1;
    if(spng.have.gama) spng_get_gama_int(ctx, &gamma_int);
    if(!spng_get_iccp(ctx, &iccp)) spng.have.iccp = 1;
    if(!spng_get_sbit(ctx, &sbit)) spng.have.sbit = 1;
    if(!spng_get_srgb(ctx, &srgb_rendering_intent)) spng.have.srgb = 1;
    if(!spng_get_text(ctx, NULL, &spng.n_text)) spng.have.text = 1;
    if(!spng_get_bkgd(ctx, &bkgd)) spng.have.bkgd = 1;
    if(!spng_get_hist(ctx, &hist)) spng.have.hist = 1;
    if(!spng_get_phys(ctx, &phys)) spng.have.phys = 1;
    if(!spng_get_splt(ctx, NULL, &spng.n_splt)) spng.have.splt = 1;
    if(!spng_get_time(ctx, &time)) spng.have.time = 1;
    if(!spng_get_offs(ctx, &offs)) spng.have.offs = 1;
    if(!spng_get_exif(ctx, &exif)) spng.have.exif = 1;
    if(!spng_get_unknown_chunks(ctx, NULL, &spng.n_unknown_chunks)) spng.have.unknown = 1;

    png.have.ihdr = 1;

    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_PLTE)) png.have.plte = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) png.have.trns = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_cHRM)) png.have.chrm = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_gAMA)) png.have.gama = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_iCCP)) png.have.iccp = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_sBIT)) png.have.sbit = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_sRGB)) png.have.srgb = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_bKGD)) png.have.bkgd = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_hIST)) png.have.hist = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_pHYs)) png.have.phys = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tIME)) png.have.time = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_oFFs)) png.have.offs = 1;
    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_eXIf)) png.have.exif = 1;

    png.have.unknown = spng.have.unknown;

    png_text *png_text;
    png.n_text = png_get_text(png_ptr, info_ptr, &png_text, NULL);
    if(png.n_text) png.have.text = 1;

    png_sPLT_t *png_splt_entries;
    png.n_splt = png_get_sPLT(png_ptr, info_ptr, &png_splt_entries);
    if(png.n_splt) png.have.splt = 1;

    const char *pos = after_idat ? " after IDAT" : "before IDAT";

    printf("[%s] spng chunks:  ", pos);
    print_chunks(spng.have);
    printf("\n");

    printf("[%s] libpng chunks:", pos);
    print_chunks(png.have);
    printf("\n");

    if(memcmp(&spng.have, &png.have, sizeof(spngt_chunk_bitfield)))
    {
        printf("[%s] ERROR: metadata mismatch!\n", pos);
        return 1;
    }

    struct spng_unknown_chunk *spng_chunks = NULL;

    if(spng.have.unknown)
    {
        spng_chunks = malloc(spng.n_unknown_chunks * sizeof(struct spng_unknown_chunk));

        if(!spng_chunks)
        {
            ret = 2;
            goto cleanup;
        }

        spng_get_unknown_chunks(ctx, spng_chunks, &spng.n_unknown_chunks);
    }

    /* NOTE: libpng changes or corrupts chunk data once it's past the IDAT stream,
             some checks are not done because of this. */
    uint32_t png_width, png_height;
    int png_bit_depth = 0, png_color_type = 0, png_interlace_method = 0, png_compression_method = 0, png_filter_method = 0;

    png_get_IHDR(png_ptr, info_ptr, &png_width, &png_height, &png_bit_depth,
                &png_color_type, &png_interlace_method, &png_compression_method,
                &png_filter_method);

    if(ihdr.width != png_width || ihdr.height != png_height || ihdr.bit_depth != png_bit_depth ||
       ihdr.color_type != png_color_type || ihdr.interlace_method != png_interlace_method ||
       ihdr.compression_method != png_compression_method || ihdr.filter_method != png_filter_method)
    {
        if(!after_idat)
        {
            printf("IHDR data not identical\n");
            ret = 1;
            goto cleanup;
        }
    }

    if(spng.have.plte)
    {
        png_color *png_palette;
        int png_num_palette;

        png_get_PLTE(png_ptr, info_ptr, &png_palette, &png_num_palette);

        if(plte.n_entries == png_num_palette)
        {
            int i;
            for(i=0; i > plte.n_entries; i++)
            {
                if(plte.entries[i].red != png_palette->red ||
                   plte.entries[i].green != png_palette->green ||
                   plte.entries[i].blue != png_palette->blue)
                {
                    printf("palette entry %d not identical\n", i);
                    ret = 1;
                }
            }
        }
        else
        {
            printf("different number of palette entries\n");
            ret = 1;
        }
    }

    if(spng.have.trns)
    {
        png_byte *png_trans_alpha;
        int png_num_trans;
        png_color_16 *png_trans_color;

        png_get_tRNS(png_ptr, info_ptr, &png_trans_alpha, &png_num_trans, &png_trans_color);

        if(ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE)
        {
            if(trns.gray != png_trans_color->gray)
            {
                printf("tRNS gray sample is not identical\n");
                ret = 1;
            }
        }
        else if(ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR)
        {
            if(trns.red != png_trans_color->red ||
            trns.green != png_trans_color->green ||
            trns.blue != png_trans_color->blue)
            {
                printf("tRNS truecolor samples not identical\n");
                ret = 1;
            }
        }
        else if(ihdr.color_type == SPNG_COLOR_TYPE_INDEXED)
        {
            if(trns.n_type3_entries == png_num_trans)
            {
                int i;
                for(i=0; i < png_num_trans; i++)
                {
                    if(trns.type3_alpha[i] != png_trans_alpha[i])
                    {
                        printf("tRNS alpha entry %d not identical\n", i);
                        ret = 1;
                    }
                }
            }
            else
            {
                if(!after_idat)
                {
                    printf("different number of tRNS alpha entries\n");
                    ret = 1;
                }
            }
        }

    }

    if(spng.have.chrm)
    {
        png_fixed_point png_fwhite_x, png_fwhite_y, png_fred_x, png_fred_y, png_fgreen_x, png_fgreen_y,
                        png_fblue_x, png_fblue_y;

        png_get_cHRM_fixed(png_ptr, info_ptr, &png_fwhite_x, &png_fwhite_y, &png_fred_x, &png_fred_y,
                           &png_fgreen_x, &png_fgreen_y, &png_fblue_x, &png_fblue_y);

        if(chrm_int.white_point_x != png_fwhite_x || chrm_int.white_point_y != png_fwhite_y ||
           chrm_int.red_x != png_fred_x || chrm_int.red_y != png_fred_y ||
           chrm_int.green_x != png_fgreen_x || chrm_int.green_y != png_fgreen_y ||
           chrm_int.blue_x != png_fblue_x || chrm_int.blue_y != png_fblue_y)
        {
            printf("cHRM fixed point values are not identical\n");
            ret = 1;
        }
    }

    if(spng.have.gama)
    {
        png_fixed_point png_fgamna;

        png_get_gAMA_fixed(png_ptr, info_ptr, &png_fgamna);

        if(gamma_int != png_fgamna)
        {
            printf("gamma values not identical\n");
            ret = 1;
        }
    }

    if(spng.have.iccp)
    {
        png_charp png_iccp_name;
        int png_iccp_compression_type;
        png_bytep png_iccp_profile;
        png_uint_32 png_iccp_proflen;

        png_get_iCCP(png_ptr, info_ptr, &png_iccp_name, &png_iccp_compression_type, &png_iccp_profile, &png_iccp_proflen);

        if(iccp.profile_len == png_iccp_proflen)
        {
            if(memcmp(iccp.profile, png_iccp_profile, iccp.profile_len))
            {
                printf("iccp profile data not identical\n");
                ret = 1;
            }
        }
        else
        {
            printf("iccp profile lengths are different\n");
            ret = 1;
        }
    }

    if(spng.have.sbit)
    {
        png_color_8p png_sig_bit;

        png_get_sBIT(png_ptr, info_ptr, &png_sig_bit);

        if(ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE && sbit.grayscale_bits != png_sig_bit->gray)
        {
            printf("grayscale significant bits not identical\n");
            ret = 1;
        }
        else if(ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR || ihdr.color_type == 3)
        {
            if(sbit.red_bits != png_sig_bit->red ||
               sbit.green_bits != png_sig_bit->green ||
               sbit.blue_bits != png_sig_bit->blue)
            {
                printf("rgb significant bits not identical\n");
                ret = 1;
            }
        }
        else if(ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE_ALPHA)
        {
            if(sbit.grayscale_bits != png_sig_bit->gray || sbit.alpha_bits != png_sig_bit->alpha)
            {
                printf("grayscale alpha significant bits not identical\n");
                ret = 1;
            }
        }
        else if(ihdr.color_type == 6)
        {
            if(sbit.red_bits != png_sig_bit->red ||
               sbit.green_bits != png_sig_bit->green ||
               sbit.blue_bits != png_sig_bit->blue ||
               sbit.alpha_bits != png_sig_bit->alpha)
            {
                printf("rgba significant bits not identical\n");
                ret = 1;
            }
        }
    }

    if(spng.have.srgb)
    {
        int png_rgb_intent;
        png_get_sRGB(png_ptr, info_ptr, &png_rgb_intent);

        if(srgb_rendering_intent != png_rgb_intent)
        {
            printf("sRGB rendering intent mismatch\n");
            ret = 1;
        }
    }

    if(spng.n_text)
    {
        text = malloc(spng.n_text * sizeof(struct spng_text));
        if(!text)
        {
            ret = 2;
            goto cleanup;
        }

        spng_get_text(ctx, text, &spng.n_text);
    }

    if(spng.n_text != png.n_text)
    {
        printf("text chunk count mismatch: %u(spng), %d(libpng)\n", spng.n_text, png.n_text);
        ret = 1;
        goto cleanup;
    }

    if(spng.n_text)
    {
        for(i=0; i < spng.n_text; i++)
        {/* TODO: other fields */
            if(strcmp(text[i].text, png_text[i].text))
            {
                printf("text[%d]: text mismatch!\nspng: %s\n\nlibpng: %s\n", i, text[i].text, png_text[i].text);
                ret = 1;
            }
        }
    }

    if(spng.have.bkgd)
    {
        png_color_16p png_bkgd;

        png_get_bKGD(png_ptr, info_ptr, &png_bkgd);

        if(ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE || ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE_ALPHA)
        {
            if(bkgd.gray != png_bkgd->gray)
            {
                printf("bKGD grayscale samples are not identical\n");
                ret = 1;
            }
        }
        else if(ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR || ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR_ALPHA)
        {
            if(bkgd.red != png_bkgd->red || bkgd.green != png_bkgd->green || bkgd.blue != png_bkgd->blue)
            {
                printf("bKGD rgb samples are not identical\n");
                ret = 1;
            }
        }
        else if(ihdr.color_type == SPNG_COLOR_TYPE_INDEXED)
        {
            if(bkgd.plte_index != png_bkgd->index)
            {
                printf("bKGD type3 indices are not identical\n");
                ret = 1;
            }
        }
    }

    if(spng.have.hist)
    {
        png_uint_16p png_hist;

        png_get_hIST(png_ptr, info_ptr, &png_hist);

        int i;
        for(i=0; i < plte.n_entries; i++)
        {
            if(hist.frequency[i] != png_hist[i])
            {
                printf("histogram entry %d is not identical\n", i);
                ret = 1;
            }
        }
    }

    if(spng.have.phys)
    {
        uint32_t png_phys_res_x, png_phys_rex_y; int png_phys_unit_type;

        png_get_pHYs(png_ptr, info_ptr, &png_phys_res_x, &png_phys_rex_y, &png_phys_unit_type);

        if(png_phys_res_x != phys.ppu_x ||
           png_phys_rex_y != phys.ppu_y ||
           png_phys_unit_type != phys.unit_specifier)
        {
            printf("pHYs data not indentical\n");
            ret = 1;
        }
    }

    if(spng.have.splt)
    {
        splt = malloc(spng.n_splt * sizeof(struct spng_splt));
        if(!splt)
        {
            ret = 2;
            goto cleanup;
        }

        spng_get_splt(ctx, splt, &spng.n_splt);
    }

    if(spng.have.splt && !after_idat) /* libpng seems to corrupt png_splt[].name */
    {
        png_sPLT_t *png_splt;

        int png_n_palettes = png_get_sPLT(png_ptr, info_ptr, &png_splt);

        if(spng.n_splt == png_n_palettes)
        {
            int i, j;

            for(j=0; j < spng.n_splt; j++)
            {
                if(strcmp(splt[j].name, png_splt[j].name))
                {
                    printf("sPLT[%d]: name mismatch\n", j);
                    ret = 1;
                }

                if(splt[j].sample_depth != png_splt[i].depth)
                {
                    printf("sPLT[%d]: sample depth mismatch\n", j);
                    ret = 1;
                }

                if(splt[j].n_entries != png_splt[j].nentries)
                {
                    printf("sPLT[%d]: entry count mismatch\n", j);
                    ret = 1;
                    break;
                }

                struct spng_splt_entry entry;
                png_sPLT_entry png_entry;

                for(i=0; i < splt[j].n_entries; i++)
                {
                    entry = splt[j].entries[i];
                    png_entry = png_splt[j].entries[i];

                    if(entry.alpha != png_entry.alpha ||
                       entry.red != png_entry.red ||
                       entry.green != png_entry.green ||
                       entry.blue != png_entry.blue ||
                       entry.frequency != png_entry.frequency)
                    {
                        printf("sPLT[%d]: mismatch for entry %d\n", j, i);
                        ret = 1;
                    }
                }
            }
        }
        else
        {
            printf("different number of suggested palettes\n");
            ret = 1;
        }
    }

    if(spng.have.time)
    {
        png_time *png_time;

        png_get_tIME(png_ptr, info_ptr, &png_time);

        if(time.year != png_time->year ||
           time.month != png_time->month ||
           time.day != png_time->day ||
           time.hour != png_time->hour ||
           time.minute != png_time->minute ||
           time.second != png_time->second)
        {
            printf("tIME data not identical\n");
            ret = 1;
        }
    }

    if(spng.have.offs)
    {
        png_int_32 png_offset_x, png_offset_y;
        int png_offs_unit_type;

        png_get_oFFs(png_ptr, info_ptr, &png_offset_x, &png_offset_y, &png_offs_unit_type);

        if(offs.x != png_offset_x || offs.y != png_offset_y || offs.unit_specifier != png_offs_unit_type)
        {
            printf("oFFs data not identical\n");
            ret = 1;
        }
    }

    if(spng.have.exif)
    {
        png_byte *png_exif;
        png_uint_32 png_exif_length;

        png_get_eXIf_1(png_ptr, info_ptr, &png_exif_length, &png_exif);

        if(exif.length == png_exif_length)
        {
            if(memcmp(exif.data, png_exif, exif.length))
            {
                printf("eXIf data not identical\n");
                ret = 1;
            }
        }
        else
        {
            printf("eXIf chunk length mismatch\n");
            ret = 1;
        }
    }

    if(spng.have.unknown)
    {
        png_unknown_chunkp png_chunks;

        png.n_unknown_chunks = png_get_unknown_chunks(png_ptr, info_ptr, &png_chunks);

        if(png.n_unknown_chunks != spng.n_unknown_chunks)
        {
            printf("unknown chunk count mismatch: %u(spng), %d(libpng)\n", spng.n_unknown_chunks, png.n_unknown_chunks);
            ret = 1;
            goto cleanup;
        }

        for(i=0; i < spng.n_unknown_chunks; i++)
        {
            if(spng_chunks[i].length != png_chunks[i].size)
            {
                printf("chunk[%d]: size mismatch %" PRIu64 "(spng) %" PRIu64" (libpng)\n", i, spng_chunks[i].length, png_chunks[i].size);
                ret = 1;
            }
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
    size_t img_spng_size;
    unsigned char *img_spng =  NULL;

    struct spngt_test_case spng_test_case =
    {
        .source.type = SPNGT_SRC_FILE,
        .test_flags = test_flags,
        .flags = flags,
        .fmt = fmt,
    };

    struct spngt_test_case png_test_case = spng_test_case;

    png_infop info_ptr = NULL;
    png_structp png_ptr = NULL;
    size_t img_png_size;
    unsigned char *img_png = NULL;

    spng_test_case.source.file = fopen(filename, "rb");
    png_test_case.source.file = fopen(filename, "rb");

    if(!spng_test_case.source.file || !png_test_case.source.file)
    {
        ret = 1;
        goto cleanup;
    }

    struct spng_ihdr ihdr;
    ctx = init_spng(&spng_test_case, &ihdr);

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

    png_ptr = init_libpng(&png_test_case, &info_ptr);
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

    if(spng_test_case.source.file) fclose(spng_test_case.source.file);
    if(png_test_case.source.file) fclose(png_test_case.source.file);

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
