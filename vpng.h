#ifndef VPNG_H
#define VPNG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>

#define VPNG_OK 0
#define VPNG_EINVAL 1
#define VPNG_EMEM 2
#define VPNG_EOVERFLOW 3
#define VPNG_ESIGNATURE 4
#define VPNG_EWIDTH 5
#define VPNG_EHEIGHT 6
#define VPNG_EUSER_WIDTH 7
#define VPNG_EUSER_HEIGHT 8
#define VPNG_EBIT_DEPTH 9
#define VPNG_ECOLOUR_TYPE 10
#define VPNG_ECOMPRESSION_METHOD 11
#define VPNG_EFILTER_METHOD 12
#define VPNG_EINTERLACE_METHOD 13
#define VPNG_EIHDR_SIZE 14
#define VPNG_ENOIHDR 15
#define VPNG_ECHUNK_POS 50
#define VPNG_ECHUNK_SIZE 51
#define VPNG_ECHUNK_CRC 52
#define VPNG_ECHUNK_TYPE 53
#define VPNG_ECHUNK_UNKNOWN_CRITICAL 54
#define VPNG_EDUP_PLTE 55
#define VPNG_EDUP_CHRM 56
#define VPNG_EDUP_GAMA 57
#define VPNG_EDUP_ICCP 58
#define VPNG_EDUP_SBIT 59
#define VPNG_EDUP_SRGB 60
#define VPNG_EDUP_BKGD 61
#define VPNG_EDUP_HIST 62
#define VPNG_EDUP_TRNS 63
#define VPNG_EDUP_PHYS 64
#define VPNG_EDUP_TIME 65
#define VPNG_ESBIT 66
#define VPNG_EPLTE_IDX 67
#define VPNG_ETRNS_COLOUR_TYPE 68
#define VPNG_EIDAT_TOO_SHORT 80
#define VPNG_EIDAT_TOO_LONG 81
#define VPNG_EDATA_AFTER_IEND 82
#define VPNG_EIDAT_STREAM 83
#define VPNG_EZLIB 84
#define VPNG_EFILTER 85
#define VPNG_EBKGD_NO_PLTE 100
#define VPNG_EBKGD_PLTE_IDX 101
#define VPNG_EBUFSIZ 120
#define VPNG_EIO 130
#define VPNG_EOF 131
#define VPNG_EBUF_SET 140
#define VPNG_EBADSTATE 141
#define VPNG_EFMT 142

#define VPNG_COLOUR_TYPE_GRAYSCALE 0
#define VPNG_COLOUR_TYPE_TRUECOLOR 2
#define VPNG_COLOUR_TYPE_INDEXED_COLOUR 3
#define VPNG_COLOUR_TYPE_GRAYSCALE_WITH_ALPHA 4
#define VPNG_COLOUR_TYPE_TRUECOLOR_WITH_ALPHA 6

#define VPNG_IO_EOF -1
#define VPNG_IO_ERROR -2

#define VPNG_FMT_PNG 1
#define VPNG_FMT_RGBA8 2
#define VPNG_FMT_RGBA16 3


struct vpng_ihdr
{
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t colour_type;
    uint8_t compression_method;
    uint8_t filter_method;
    uint8_t interlace_method;
};

struct vpng_plte_entry
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct vpng_trns_type2
{
    uint16_t red;
    uint16_t green;
    uint16_t blue;
};

struct vpng_sbit_type2_3
{
    uint8_t red_bits;
    uint8_t green_bits;
    uint8_t blue_bits;
};

struct vpng_sbit_type4
{
    uint8_t greyscale_bits;
    uint8_t alpha_bits;
};

struct vpng_sbit_type6
{
    uint8_t red_bits;
    uint8_t green_bits;
    uint8_t blue_bits;
    uint8_t alpha_bits;
};

struct vpng_bkgd_type2_6
{
    uint16_t red;
    uint16_t green;
    uint16_t blue;
};

struct vpng_chrm
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

struct vpng_time
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

struct vpng_chunk
{
    size_t offset;
    uint32_t length;
    uint8_t type[4];
    uint32_t crc;
};

struct vpng_decoder;

/* A read callback function should copy "n" bytes to *data and return 0 or
   vpng_io_eof/vpng_io_error on error. */
typedef int (*vpng_read_fn)(struct vpng_decoder *dec, void *user, void *data, size_t n);


struct vpng_decoder
{
    size_t data_size;
    unsigned char *data;

    int valid_state;

    struct vpng_chunk first_idat, last_idat;

    int8_t have_ihdr;
    struct vpng_ihdr ihdr;

    int8_t have_plte;
    size_t plte_offset;
    uint32_t n_plte_entries;
    struct vpng_plte_entry plte_entries[256];

    uint8_t have_chrm;
    struct vpng_chrm chrm;

    uint8_t have_gama;
    uint32_t gama;

    uint8_t have_iccp;

    uint8_t have_sbit;
    union
    {
        uint8_t sbit_type0_greyscale_bits;
        struct vpng_sbit_type2_3 sbit_type2_3;
        struct vpng_sbit_type4 sbit_type4;
        struct vpng_sbit_type6 sbit_type6;
    };

    uint8_t have_srgb;
    uint8_t srgb_rendering_intent;

    uint8_t have_bkgd;
    union
    {
        uint16_t bkgd_type0_4_greyscale;
        struct vpng_bkgd_type2_6 bkgd_type2_6;
        uint8_t bkgd_type3_plte_index;
    };

    uint8_t have_hist;
    uint16_t hist_frequency[256];

    uint8_t have_trns;
    uint32_t n_trns_type3_entries;
    union
    {
        uint16_t trns_type0_grey_sample;
        struct vpng_trns_type2 trns_type2;
        uint8_t trns_type3_alpha[256];
    };

    uint8_t have_phys;
    uint32_t phys_ppu_x, phys_ppu_y;
    uint32_t phys_unit_specifier;

    uint8_t have_time;
    struct vpng_time time;
};


extern struct vpng_decoder *vpng_decoder_new(void);
extern void vpng_decoder_free(struct vpng_decoder *dec);

/* Sets a full input buffer for decoding. */
extern int vpng_decoder_set_buffer(struct vpng_decoder *dec, void *buf, size_t size);

/* Copies image information to *ihdr, requires an input buffer to be set. */
extern int vpng_get_ihdr(struct vpng_decoder *dec, struct vpng_ihdr *ihdr);

/* Sets the value of *out to the required size, fmt must be one of VPNG_FMT_*
*/
extern int vpng_get_output_image_size(struct vpng_decoder *dec, int fmt, size_t *out);

extern int vpng_decode_image(struct vpng_decoder *dec, int fmt, unsigned char *out, size_t out_size, int flags);

#ifdef __cplusplus
}
#endif

#endif /* VPNG_H */
