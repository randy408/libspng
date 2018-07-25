#ifndef SPNG_H
#define SPNG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>

#define SPNG_VERSION_MAJOR 0
#define SPNG_VERSION_MINOR 2
#define SPNG_VERSION_PATCH 0

enum
{
    SPNG_OK = 0,
    SPNG_EINVAL,
    SPNG_EMEM,
    SPNG_EOVERFLOW,
    SPNG_ESIGNATURE,
    SPNG_EWIDTH,
    SPNG_EHEIGHT,
    SPNG_EUSER_WIDTH,
    SPNG_EUSER_HEIGHT,
    SPNG_EBIT_DEPTH,
    SPNG_ECOLOUR_TYPE,
    SPNG_ECOMPRESSION_METHOD,
    SPNG_EFILTER_METHOD,
    SPNG_EINTERLACE_METHOD,
    SPNG_EIHDR_SIZE,
    SPNG_ENOIHDR,
    SPNG_ECHUNK_POS,
    SPNG_ECHUNK_SIZE,
    SPNG_ECHUNK_CRC,
    SPNG_ECHUNK_TYPE,
    SPNG_ECHUNK_UNKNOWN_CRITICAL,
    SPNG_EDUP_PLTE,
    SPNG_EDUP_CHRM,
    SPNG_EDUP_GAMA,
    SPNG_EDUP_ICCP,
    SPNG_EDUP_SBIT,
    SPNG_EDUP_SRGB,
    SPNG_EDUP_BKGD,
    SPNG_EDUP_HIST,
    SPNG_EDUP_TRNS,
    SPNG_EDUP_PHYS,
    SPNG_EDUP_TIME,
    SPNG_EDUP_OFFS,
    SPNG_EDUP_EXIF,
    SPNG_ECHRM,
    SPNG_EPLTE_IDX,
    SPNG_ETRNS_COLOUR_TYPE,
    SPNG_ETRNS_NO_PLTE,
    SPNG_EGAMA,
    SPNG_EICCP_NAME,
    SPNG_EICCP_COMPRESSION_METHOD,
    SPNG_ESBIT,
    SPNG_ESRGB,
    SPNG_ETEXT,
    SPNG_ETEXT_KEYWORD,
    SPNG_EZTXT,
    SPNG_EZTXT_COMPRESSION_METHOD,
    SPNG_EITXT,
    SPNG_EITXT_COMPRESSION_FLAG,
    SPNG_EITXT_COMPRESSION_METHOD,
    SPNG_EITXT_LANG_TAG,
    SPNG_EITXT_TRANSLATED_KEY,
    SPNG_EBKGD_NO_PLTE,
    SPNG_EBKGD_PLTE_IDX,
    SPNG_EHIST_NO_PLTE,
    SPNG_EPHYS,
    SPNG_ESPLT_NAME,
    SPNG_ESPLT_DUP_NAME,
    SPNG_ESPLT_DEPTH,
    SPNG_ETIME,
    SPNG_EOFFS,
    SPNG_EEXIF,
    SPNG_EIDAT_TOO_SHORT,
    SPNG_EIDAT_STREAM,
    SPNG_EZLIB,
    SPNG_EFILTER,
    SPNG_EBUFSIZ,
    SPNG_EIO,
    SPNG_EOF,
    SPNG_EBUF_SET,
    SPNG_EBADSTATE,
    SPNG_EFMT,
    SPNG_EFLAGS,
    SPNG_ECHUNKAVAIL,
    SPNG_EINCOMPLETE
};


#define SPNG_TEXT 1
#define SPNG_ZTXT 2
#define SPNG_ITXT 3

#define SPNG_COLOUR_TYPE_GRAYSCALE 0
#define SPNG_COLOUR_TYPE_TRUECOLOR 2
#define SPNG_COLOUR_TYPE_INDEXED 3
#define SPNG_COLOUR_TYPE_GRAYSCALE_ALPHA 4
#define SPNG_COLOUR_TYPE_TRUECOLOR_ALPHA 6

#define SPNG_IO_EOF -1
#define SPNG_IO_ERROR -2

#define SPNG_FMT_RGBA8 1
#define SPNG_FMT_RGBA16 2

#define SPNG_DECODE_USE_TRNS 1
#define SPNG_DECODE_USE_GAMA 2
#define SPNG_DECODE_USE_SBIT 8 /* Rescale samples using sBIT values */

struct spng_ihdr
{
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t colour_type;
    uint8_t compression_method;
    uint8_t filter_method;
    uint8_t interlace_method;
};

struct spng_plte_entry
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct spng_plte
{
    uint32_t n_entries;
    struct spng_plte_entry entries[256];
};

struct spng_trns_type2
{
    uint16_t red;
    uint16_t green;
    uint16_t blue;
};

struct spng_trns
{
    uint32_t n_type3_entries;
    union
    {
        uint16_t type0_grey_sample;
        struct spng_trns_type2 type2;
        uint8_t type3_alpha[256];
    };
};

struct spng_chrm
{
    uint32_t white_point_x;
    uint32_t white_point_y;
    uint32_t red_x;
    uint32_t red_y;
    uint32_t green_x;
    uint32_t green_y;
    uint32_t blue_x;
    uint32_t blue_y;
};

struct spng_iccp
{
    char profile_name[80];
    size_t profile_len;
    char *profile;
};

struct spng_sbit
{
    uint8_t greyscale_bits;
    uint8_t red_bits;
    uint8_t green_bits;
    uint8_t blue_bits;
    uint8_t alpha_bits;
};

struct spng_text
{
    char keyword[80];
    int type;

    size_t length;
    char *text;

    uint8_t compression_flag; /* iTXt only */
    uint8_t compression_method; /* iTXt, ztXt only */
    char *language_tag; /* iTXt only */
    char *translated_keyword; /* iTXt only */
};

struct spng_bkgd_type2_6
{
    uint16_t red;
    uint16_t green;
    uint16_t blue;
};

struct spng_bkgd
{
    union
    {
        uint16_t type0_4_greyscale;
        struct spng_bkgd_type2_6 type2_6;
        uint8_t type3_plte_index;
    };
};

struct spng_hist
{
    uint16_t frequency[256];
};

struct spng_phys
{
    uint32_t ppu_x, ppu_y;
    uint8_t unit_specifier;
};

struct spng_splt_entry
{
    uint16_t red;
    uint16_t green;
    uint16_t blue;
    uint16_t alpha;
    uint16_t frequency;
};

struct spng_splt
{
    char name[80];
    uint8_t sample_depth;
    uint32_t n_entries;
    struct spng_splt_entry *entries;
};

struct spng_time
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

struct spng_offs
{
    int32_t x, y;
    uint8_t unit_specifier;
};

struct spng_exif
{
    size_t length;
    char *data;
};

struct spng_chunk
{
    size_t offset;
    uint32_t length;
    uint8_t type[4];
    uint32_t crc;
};

struct spng_ctx;

/* A read callback function should copy "n" bytes to *data and return 0 or
   SPNG_IO_EOF/SPNG_IO_ERROR on error. */
typedef int spng_read_fn(struct spng_ctx *ctx, void *user, void *data, size_t n);


struct spng_ctx
{
    size_t data_size;
    unsigned char *data;

    spng_read_fn *read_fn;
    void *read_user_ptr;

    unsigned valid_state: 1;
    unsigned streaming: 1;

    unsigned have_ihdr: 1;
    unsigned have_plte: 1;
    unsigned have_chrm: 1;
    unsigned have_iccp: 1;
    unsigned user_iccp: 1;
    unsigned have_gama: 1;
    unsigned have_sbit: 1;
    unsigned have_srgb: 1;
    unsigned have_text: 1;
    unsigned user_text: 1;
    unsigned have_bkgd: 1;
    unsigned have_hist: 1;
    unsigned have_trns: 1;
    unsigned have_phys: 1;
    unsigned have_splt: 1;
    unsigned user_splt: 1;
    unsigned have_time: 1;
    unsigned file_time: 1;
    unsigned have_offs: 1;
    unsigned have_exif: 1;
    unsigned file_exif: 1;
    unsigned user_exif: 1;

    unsigned have_first_idat: 1;
    unsigned have_last_idat: 1;

    struct spng_chunk first_idat, last_idat;

    uint32_t max_width, max_height;
    struct spng_ihdr ihdr;

    size_t plte_offset;
    struct spng_plte plte;

    struct spng_chrm chrm;
    struct spng_iccp iccp;

    uint32_t gama;
    uint32_t lut_entries;
    uint16_t *gamma_lut;

    struct spng_sbit sbit;

    uint8_t srgb_rendering_intent;

    uint32_t n_text;
    struct spng_text *text_list;

    struct spng_bkgd bkgd;
    struct spng_hist hist;
    struct spng_trns trns;
    struct spng_phys phys;

    uint32_t n_splt;
    struct spng_splt *splt_list;

    struct spng_time time;
    struct spng_offs offs;
    struct spng_exif exif;
};


struct spng_ctx *spng_ctx_new(void);
void spng_ctx_free(struct spng_ctx *ctx);

int spng_set_png_buffer(struct spng_ctx *ctx, void *buf, size_t size);
int spng_set_png_stream(struct spng_ctx *ctx, spng_read_fn *read_fn, void *user);

int spng_set_image_limits(struct spng_ctx *ctx, uint32_t width, uint32_t height);
int spng_get_image_limits(struct spng_ctx *ctx, uint32_t *width, uint32_t *height);

int spng_decoded_image_size(struct spng_ctx *ctx, int fmt, size_t *out);

int spng_decode_image(struct spng_ctx *ctx, void *out, size_t out_size, int fmt, int flags);

int spng_get_ihdr(struct spng_ctx *ctx, struct spng_ihdr *ihdr);
int spng_get_plte(struct spng_ctx *ctx, struct spng_plte *plte);
int spng_get_trns(struct spng_ctx *ctx, struct spng_trns *trns);
int spng_get_chrm(struct spng_ctx *ctx, struct spng_chrm *chrm);
int spng_get_gama(struct spng_ctx *ctx, double *gamma);
int spng_get_iccp(struct spng_ctx *ctx, struct spng_iccp *iccp);
int spng_get_sbit(struct spng_ctx *ctx, struct spng_sbit *sbit);
int spng_get_srgb(struct spng_ctx *ctx, uint8_t *rendering_intent);
int spng_get_text(struct spng_ctx *ctx, struct spng_text *text, uint32_t *n_text);
int spng_get_bkgd(struct spng_ctx *ctx, struct spng_bkgd *bkgd);
int spng_get_hist(struct spng_ctx *ctx, struct spng_hist *hist);
int spng_get_phys(struct spng_ctx *ctx, struct spng_phys *phys);
int spng_get_splt(struct spng_ctx *ctx, struct spng_splt *splt, uint32_t *n_splt);
int spng_get_time(struct spng_ctx *ctx, struct spng_time *time);

int spng_get_offs(struct spng_ctx *ctx, struct spng_offs *offs);
int spng_get_exif(struct spng_ctx *ctx, struct spng_exif *exif);


int spng_set_ihdr(struct spng_ctx *ctx, struct spng_ihdr *ihdr);
int spng_set_plte(struct spng_ctx *ctx, struct spng_plte *plte);
int spng_set_trns(struct spng_ctx *ctx, struct spng_trns *trns);
int spng_set_chrm(struct spng_ctx *ctx, struct spng_chrm *chrm);
int spng_set_gama(struct spng_ctx *ctx, double gamma);
int spng_set_iccp(struct spng_ctx *ctx, struct spng_iccp *iccp);
int spng_set_sbit(struct spng_ctx *ctx, struct spng_sbit *sbit);
int spng_set_srgb(struct spng_ctx *ctx, uint8_t rendering_intent);
int spng_set_text(struct spng_ctx *ctx, struct spng_text *text, uint32_t n_text);
int spng_set_bkgd(struct spng_ctx *ctx, struct spng_bkgd *bkgd);
int spng_set_hist(struct spng_ctx *ctx, struct spng_hist *hist);
int spng_set_phys(struct spng_ctx *ctx, struct spng_phys *phys);
int spng_set_splt(struct spng_ctx *ctx, struct spng_splt *splt, uint32_t n_splt);
int spng_set_time(struct spng_ctx *ctx, struct spng_time *time);

int spng_set_offs(struct spng_ctx *ctx, struct spng_offs *offs);
int spng_set_exif(struct spng_ctx *ctx, struct spng_exif *exif);

#ifdef __cplusplus
}
#endif

#endif /* SPNG_H */
