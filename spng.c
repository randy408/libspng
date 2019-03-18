#include "spng.h"

#include <limits.h>
#include <string.h>
#include <math.h>

#include <zlib.h>

#define SPNG_READ_SIZE 8192

#if defined(SPNG_OPTIMIZE_FILTER)
    #if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
        #define SPNG_X86
    #else
        #undef SPNG_OPTIMIZE_FILTER
    #endif

    void png_read_filter_row_sub3(size_t rowbytes, unsigned char* row);
    void png_read_filter_row_sub4(size_t rowbytes, unsigned char* row);
    void png_read_filter_row_avg3(size_t rowbytes, unsigned char* row, const unsigned char* prev);
    void png_read_filter_row_avg4(size_t rowbytes, unsigned char* row, const unsigned char* prev);
    void png_read_filter_row_paeth3(size_t rowbytes, unsigned char* row, const unsigned char* prev);
    void png_read_filter_row_paeth4(size_t rowbytes, unsigned char* row, const unsigned char* prev);
#endif

#define SPNG_FILTER_TYPE_NONE 0
#define SPNG_FILTER_TYPE_SUB 1
#define SPNG_FILTER_TYPE_UP 2
#define SPNG_FILTER_TYPE_AVERAGE 3
#define SPNG_FILTER_TYPE_PAETH 4

#define SPNG_STR(x) _SPNG_STR(x)
#define _SPNG_STR(x) #x

#define SPNG_VERSION_STRING SPNG_STR(SPNG_VERSION_MAJOR) "." \
                            SPNG_STR(SPNG_VERSION_MINOR) "." \
                            SPNG_STR(SPNG_VERSION_PATCH)

#define SPNG_GET_CHUNK_BOILERPLATE(chunk) \
    if(ctx == NULL || chunk == NULL) return 1; \
    int ret = get_ancillary(ctx); \
    if(ret) return ret;

#define SPNG_SET_CHUNK_BOILERPLATE(chunk) \
    if(ctx == NULL || chunk == NULL) return 1; \
    int ret = get_ancillary2(ctx); \
    if(ret) return ret;

struct spng_ctx
{
    size_t data_size;
    size_t bytes_read;
    unsigned char *data;

    /* User-defined pointers for streaming */
    spng_read_fn *read_fn;
    void *read_user_ptr;

    /* Used for buffer reads */
    unsigned char *png_buf; /* base pointer for the buffer */
    size_t bytes_left;
    size_t last_read_size;

    /* These are updated by read_header()/read_chunk_bytes() */
    struct spng_chunk current_chunk;
    uint32_t cur_chunk_bytes_left;
    uint32_t cur_actual_crc;

    struct spng_alloc alloc;

    unsigned valid_state: 1;
    unsigned streaming: 1;

    unsigned encode_only: 1;

/* input file contains this chunk */
    unsigned file_ihdr: 1;
    unsigned file_plte: 1;
    unsigned file_chrm: 1;
    unsigned file_iccp: 1;
    unsigned file_gama: 1;
    unsigned file_sbit: 1;
    unsigned file_srgb: 1;
    unsigned file_text: 1;
    unsigned file_bkgd: 1;
    unsigned file_hist: 1;
    unsigned file_trns: 1;
    unsigned file_phys: 1;
    unsigned file_splt: 1;
    unsigned file_time: 1;
    unsigned file_offs: 1;
    unsigned file_exif: 1;

/* chunk was stored with spng_set_*() */
    unsigned user_ihdr: 1;
    unsigned user_plte: 1;
    unsigned user_chrm: 1;
    unsigned user_iccp: 1;
    unsigned user_gama: 1;
    unsigned user_sbit: 1;
    unsigned user_srgb: 1;
    unsigned user_text: 1;
    unsigned user_bkgd: 1;
    unsigned user_hist: 1;
    unsigned user_trns: 1;
    unsigned user_phys: 1;
    unsigned user_splt: 1;
    unsigned user_time: 1;
    unsigned user_offs: 1;
    unsigned user_exif: 1;

/* chunk was stored by reading or with spng_set_*() */
    unsigned stored_ihdr: 1;
    unsigned stored_plte: 1;
    unsigned stored_chrm: 1;
    unsigned stored_iccp: 1;
    unsigned stored_gama: 1;
    unsigned stored_sbit: 1;
    unsigned stored_srgb: 1;
    unsigned stored_text: 1;
    unsigned stored_bkgd: 1;
    unsigned stored_hist: 1;
    unsigned stored_trns: 1;
    unsigned stored_phys: 1;
    unsigned stored_splt: 1;
    unsigned stored_time: 1;
    unsigned stored_offs: 1;
    unsigned stored_exif: 1;

    unsigned have_first_idat: 1;
    unsigned have_last_idat: 1;

    struct spng_chunk first_idat, last_idat;

    uint32_t max_width, max_height;

    uint32_t max_chunk_size;
    size_t chunk_cache_limit;
    size_t chunk_cache_usage;

    int crc_action_critical;
    int crc_action_ancillary;

    struct spng_ihdr ihdr;

    size_t plte_offset;
    struct spng_plte plte;

    struct spng_chrm_int chrm_int;
    struct spng_iccp iccp;

    uint32_t gama;
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


struct spng_subimage
{
    uint32_t width;
    uint32_t height;
    size_t scanline_width;
};

void *spng__malloc(spng_ctx *ctx,  size_t size);
void *spng__calloc(spng_ctx *ctx, size_t nmemb, size_t size);
void *spng__realloc(spng_ctx *ctx, void *ptr, size_t size);
void spng__free(spng_ctx *ctx, void *ptr);

int get_ancillary(spng_ctx *ctx);
int get_ancillary2(spng_ctx *ctx);

int calculate_subimages(struct spng_subimage sub[7], size_t *widest_scanline, struct spng_ihdr *ihdr, unsigned channels);

int check_ihdr(struct spng_ihdr *ihdr, uint32_t max_width, uint32_t max_height);
int check_sbit(struct spng_sbit *sbit, struct spng_ihdr *ihdr);
int check_chrm_int(struct spng_chrm_int *chrm_int);
int check_phys(struct spng_phys *phys);
int check_time(struct spng_time *time);
int check_offs(struct spng_offs *offs);
int check_exif(struct spng_exif *exif);

int check_png_keyword(const char str[80]);
int check_png_text(const char *str, size_t len);

static const uint32_t png_u32max = 2147483647;
static const int32_t png_s32min = -2147483647;

static const uint8_t type_ihdr[4] = { 73, 72, 68, 82 };
static const uint8_t type_plte[4] = { 80, 76, 84, 69 };
static const uint8_t type_idat[4] = { 73, 68, 65, 84 };
static const uint8_t type_iend[4] = { 73, 69, 78, 68 };

static const uint8_t type_trns[4] = { 116, 82, 78, 83 };
static const uint8_t type_chrm[4] = { 99,  72, 82, 77 };
static const uint8_t type_gama[4] = { 103, 65, 77, 65 };
static const uint8_t type_iccp[4] = { 105, 67, 67, 80 };
static const uint8_t type_sbit[4] = { 115, 66, 73, 84 };
static const uint8_t type_srgb[4] = { 115, 82, 71, 66 };
static const uint8_t type_text[4] = { 116, 69, 88, 116 };
static const uint8_t type_ztxt[4] = { 122, 84, 88, 116 };
static const uint8_t type_itxt[4] = { 105, 84, 88, 116 };
static const uint8_t type_bkgd[4] = { 98,  75, 71, 68 };
static const uint8_t type_hist[4] = { 104, 73, 83, 84 };
static const uint8_t type_phys[4] = { 112, 72, 89, 115 };
static const uint8_t type_splt[4] = { 115, 80, 76, 84 };
static const uint8_t type_time[4] = { 116, 73, 77, 69 };

static const uint8_t type_offs[4] = { 111, 70, 70, 115 };
static const uint8_t type_exif[4] = { 101, 88, 73, 102 };

static inline uint16_t read_u16(const void *_data)
{
    const unsigned char *data = _data;

    return (data[0] & 0xffU) << 8 | (data[1] & 0xffU);
}

static inline uint32_t read_u32(const void *_data)
{
    const unsigned char *data = _data;

    return (data[0] & 0xffUL) << 24 | (data[1] & 0xffUL) << 16 |
           (data[2] & 0xffUL) << 8  | (data[3] & 0xffUL);
}

static inline int32_t read_s32(const void *_data)
{
    const unsigned char *data = _data;

    int32_t ret;
    uint32_t val = (data[0] & 0xffUL) << 24 | (data[1] & 0xffUL) << 16 |
                   (data[2] & 0xffUL) << 8  | (data[3] & 0xffUL);

    memcpy(&ret, &val, 4);

    return ret;
}

int is_critical_chunk(struct spng_chunk *chunk)
{
    if(chunk == NULL) return 0;
    if((chunk->type[0] & (1 << 5)) == 0) return 1;

    return 0;
}

static inline int read_data(spng_ctx *ctx, size_t bytes)
{
    if(ctx == NULL) return 1;
    if(!bytes) return 0;

    if(ctx->streaming && (bytes > ctx->data_size))
    {
        void *buf = spng__realloc(ctx, ctx->data, bytes);
        if(buf == NULL) return SPNG_EMEM;

        ctx->data = buf;
        ctx->data_size = bytes;
    }

    int ret;
    ret = ctx->read_fn(ctx, ctx->read_user_ptr, ctx->data, bytes);
    if(ret) return ret;

    ctx->bytes_read += bytes;
    if(ctx->bytes_read < bytes) return SPNG_EOVERFLOW;

    return 0;
}

static inline int read_and_check_crc(spng_ctx *ctx)
{
    if(ctx == NULL) return 1;

    int ret;
    ret = read_data(ctx, 4);
    if(ret) return ret;

    ctx->current_chunk.crc = read_u32(ctx->data);

    if(is_critical_chunk(&ctx->current_chunk) && ctx->crc_action_critical == SPNG_CRC_USE)
        goto skip_crc;
    else if(ctx->crc_action_ancillary == SPNG_CRC_USE)
        goto skip_crc;

    if(ctx->cur_actual_crc != ctx->current_chunk.crc) return SPNG_ECHUNK_CRC;

skip_crc:

    return 0;
}

/* Read and validate the current chunk's crc and the next chunk header */
static inline int read_header(spng_ctx *ctx)
{
    if(ctx == NULL) return 1;

    int ret;
    struct spng_chunk chunk = { 0 };

    ret = read_and_check_crc(ctx);
    if(ret) return ret;

    ret = read_data(ctx, 8);
    if(ret) return ret;

    chunk.offset = ctx->bytes_read - 8;

    chunk.length = read_u32(ctx->data);

    memcpy(&chunk.type, ctx->data + 4, 4);

    if(chunk.length > png_u32max) return SPNG_ECHUNK_SIZE;

    ctx->cur_chunk_bytes_left = chunk.length;

    ctx->cur_actual_crc = crc32(0, NULL, 0);
    ctx->cur_actual_crc = crc32(ctx->cur_actual_crc, chunk.type, 4);

    memcpy(&ctx->current_chunk, &chunk, sizeof(struct spng_chunk));

    return 0;
}

/* Read chunk bytes and update crc */
static int read_chunk_bytes(spng_ctx *ctx, uint32_t bytes)
{
    if(ctx == NULL) return 1;
    if(!bytes) return 0;
    if(bytes > ctx->cur_chunk_bytes_left) return 1; /* XXX: more specific error? */

    int ret;

    ret = read_data(ctx, bytes);
    if(ret) return ret;

    if(is_critical_chunk(&ctx->current_chunk) &&
       ctx->crc_action_critical == SPNG_CRC_USE) goto skip_crc;
    else if(ctx->crc_action_ancillary == SPNG_CRC_USE) goto skip_crc;

    ctx->cur_actual_crc = crc32(ctx->cur_actual_crc, ctx->data, bytes);
    
skip_crc:
    ctx->cur_chunk_bytes_left -= bytes;

    return ret;
}

static int discard_chunk_bytes(spng_ctx *ctx, uint32_t bytes)
{
    if(ctx == NULL) return 1;

    int ret;

    if(ctx->streaming) /* Do small, consecutive reads */
    {
        while(bytes)
        {
            uint32_t len = SPNG_READ_SIZE;

            if(len > bytes) len = bytes;

            ret = read_chunk_bytes(ctx, len);
            if(ret) return ret;

            bytes -= len;
        }
    }
    else
    {
        ret = read_chunk_bytes(ctx, bytes);
        if(ret) return ret;
    }

    return 0;
}

/* Read at least one byte from the IDAT stream */
static int get_idat_bytes(spng_ctx *ctx, uint32_t *bytes_read)
{
    if(ctx == NULL || bytes_read == NULL) return 1;
    if(memcmp(ctx->current_chunk.type, type_idat, 4)) return SPNG_EIDAT_TOO_SHORT;

    int ret;
    uint32_t len;

    while(!ctx->cur_chunk_bytes_left)
    {
        ret = read_header(ctx);
        if(ret) return ret;

        if(memcmp(ctx->current_chunk.type, type_idat, 4)) return SPNG_EIDAT_TOO_SHORT;
    }

    if(ctx->streaming)
    {/* TODO: calculate bytes to read for progressive reads */
        len = SPNG_READ_SIZE;
        if(len > ctx->current_chunk.length) len = ctx->current_chunk.length;
    }
    else len = ctx->current_chunk.length;

    ret = read_chunk_bytes(ctx, len);

    *bytes_read = len;

    return ret;
}

static uint8_t paeth(uint8_t a, uint8_t b, uint8_t c)
{
    int16_t p = (int16_t)a + (int16_t)b - (int16_t)c;
    int16_t pa = abs(p - (int16_t)a);
    int16_t pb = abs(p - (int16_t)b);
    int16_t pc = abs(p - (int16_t)c);

    if(pa <= pb && pa <= pc) return a;
    else if(pb <= pc) return b;

    return c;
}

/* Defilter *scanline in-place.
   *prev_scanline and *scanline should point to the first pixel,
   scanline_width is the width of the scanline without the filter byte.
   */
static int defilter_scanline(const unsigned char *prev_scanline, unsigned char *scanline,
                             size_t scanline_width, unsigned bytes_per_pixel, unsigned filter)
{
    if(prev_scanline == NULL || scanline == NULL || !scanline_width) return 1;

    size_t i;

    if(filter > 4) return SPNG_EFILTER;
    if(filter == 0) return 0;

#if defined(SPNG_OPTIMIZE_FILTER)
    if(filter == SPNG_FILTER_TYPE_UP) goto no_opt;

    if(bytes_per_pixel == 4)
    {
        if(filter == SPNG_FILTER_TYPE_SUB)
            png_read_filter_row_sub4(scanline_width, scanline);
        else if(filter == SPNG_FILTER_TYPE_AVERAGE)
            png_read_filter_row_avg4(scanline_width, scanline, prev_scanline);
        else if(filter == SPNG_FILTER_TYPE_PAETH)
            png_read_filter_row_paeth4(scanline_width, scanline, prev_scanline);

        return 0;
    }
    else if(bytes_per_pixel == 3)
    {
        if(filter == SPNG_FILTER_TYPE_SUB)
            png_read_filter_row_sub3(scanline_width, scanline);
        else if(filter == SPNG_FILTER_TYPE_AVERAGE)
            png_read_filter_row_avg3(scanline_width, scanline, prev_scanline);
        else if(filter == SPNG_FILTER_TYPE_PAETH)
            png_read_filter_row_paeth3(scanline_width, scanline, prev_scanline);

        return 0;
    }
no_opt:
#endif

    for(i=0; i < scanline_width; i++)
    {
        uint8_t x, a, b, c;

        if(i >= bytes_per_pixel)
        {
            memcpy(&a, scanline + i - bytes_per_pixel, 1);
            memcpy(&b, prev_scanline + i, 1);
            memcpy(&c, prev_scanline + i - bytes_per_pixel, 1);
        }
        else /* first pixel in row */
        {
            a = 0;
            memcpy(&b, prev_scanline + i, 1);
            c = 0;
        }

        memcpy(&x, scanline + i, 1);

        switch(filter)
        {
            case SPNG_FILTER_TYPE_SUB:
            {
                x = x + a;
                break;
            }
            case SPNG_FILTER_TYPE_UP:
            {
                x = x + b;
                break;
            }
            case SPNG_FILTER_TYPE_AVERAGE:
            {
                uint16_t avg = (a + b) / 2;
                x = x + avg;
                break;
            }
            case SPNG_FILTER_TYPE_PAETH:
            {
                x = x + paeth(a,b,c);
                break;
            }
        }

        memcpy(scanline + i, &x, 1);
    }

    return 0;
}

static int chunk_fits_in_cache(spng_ctx *ctx, size_t *new_usage)
{
    if(ctx == NULL || new_usage == NULL) return 0;
   
    size_t usage = ctx->chunk_cache_usage + (size_t)ctx->current_chunk.length;

    if(usage < ctx->chunk_cache_usage) return 0; /* overflow */

    if(usage > ctx->chunk_cache_limit) return 0;

    *new_usage = usage;

    return 1;
}

/*
    Read and validate all critical and relevant ancillary chunks up to the first IDAT
    Returns zero and sets ctx->first_idat on success
*/
static int get_ancillary_data_first_idat(spng_ctx *ctx)
{
    if(ctx == NULL) return 1;
    if(ctx->data == NULL) return 1;
    if(!ctx->valid_state) return SPNG_EBADSTATE;

    int ret;
    unsigned char *data;
    struct spng_chunk chunk;

    chunk.offset = 8;
    chunk.length = 13;
    size_t sizeof_sig_ihdr = 29;

    ret = read_data(ctx, sizeof_sig_ihdr);
    if(ret) return ret;

    data = ctx->data;

    uint8_t signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if(memcmp(data, signature, sizeof(signature))) return SPNG_ESIGNATURE;

    chunk.length = read_u32(data + 8);
    memcpy(&chunk.type, data + 12, 4);

    if(chunk.length != 13) return SPNG_EIHDR_SIZE;
    if(memcmp(chunk.type, type_ihdr, 4)) return SPNG_ENOIHDR;

    ctx->cur_actual_crc = crc32(0, NULL, 0);
    ctx->cur_actual_crc = crc32(ctx->cur_actual_crc, data + 12, 17);

    ctx->ihdr.width = read_u32(data + 16);
    ctx->ihdr.height = read_u32(data + 20);
    memcpy(&ctx->ihdr.bit_depth, data + 24, 1);
    memcpy(&ctx->ihdr.color_type, data + 25, 1);
    memcpy(&ctx->ihdr.compression_method, data + 26, 1);
    memcpy(&ctx->ihdr.filter_method, data + 27, 1);
    memcpy(&ctx->ihdr.interlace_method, data + 28, 1);

    if(!ctx->max_width) ctx->max_width = png_u32max;
    if(!ctx->max_height) ctx->max_height = png_u32max;

    ret = check_ihdr(&ctx->ihdr, ctx->max_width, ctx->max_height);
    if(ret) return ret;

    ctx->file_ihdr = 1;
    ctx->stored_ihdr = 1;

    while( !(ret = read_header(ctx)))
    {
        memcpy(&chunk, &ctx->current_chunk, sizeof(struct spng_chunk));

        if(!memcmp(chunk.type, type_idat, 4))
        {
            memcpy(&ctx->first_idat, &chunk, sizeof(struct spng_chunk));
            return 0;
        }

        if(!chunk_fits_in_cache(ctx, &ctx->chunk_cache_usage)) continue;

        ret = read_chunk_bytes(ctx, chunk.length);
        if(ret) return ret;

        data = ctx->data;

        /* Reserved bit must be zero */
        if( (chunk.type[2] & (1 << 5)) != 0) return SPNG_ECHUNK_TYPE;
        /* Ignore private chunks */
        if( (chunk.type[1] & (1 << 5)) != 0) continue;

        if(is_critical_chunk(&chunk)) /* Critical chunk */
        {
            if(!memcmp(chunk.type, type_plte, 4))
            {
                if(chunk.length == 0) return SPNG_ECHUNK_SIZE;
                if(chunk.length % 3 != 0) return SPNG_ECHUNK_SIZE;
                if( (chunk.length / 3) > 256 ) return SPNG_ECHUNK_SIZE;

                if(ctx->ihdr.color_type == 3)
                {
                    if(chunk.length / 3 > (1 << ctx->ihdr.bit_depth) ) return SPNG_ECHUNK_SIZE;
                }

                ctx->plte.n_entries = chunk.length / 3;

                size_t i;
                for(i=0; i < ctx->plte.n_entries; i++)
                {
                    memcpy(&ctx->plte.entries[i].red,   data + i * 3, 1);
                    memcpy(&ctx->plte.entries[i].green, data + i * 3 + 1, 1);
                    memcpy(&ctx->plte.entries[i].blue,  data + i * 3 + 2, 1);
                }

                ctx->plte_offset = chunk.offset;

                ctx->file_plte = 1;
            }
            else if(!memcmp(chunk.type, type_iend, 4)) return SPNG_ECHUNK_POS;
            else if(!memcmp(chunk.type, type_ihdr, 4)) return SPNG_ECHUNK_POS;
            else return SPNG_ECHUNK_UNKNOWN_CRITICAL;
        }
        else if(!memcmp(chunk.type, type_chrm, 4)) /* Ancillary chunks */
        {
            if(ctx->file_plte && chunk.offset > ctx->plte_offset) return SPNG_ECHUNK_POS;
            if(ctx->file_chrm) return SPNG_EDUP_CHRM;

            if(chunk.length != 32) return SPNG_ECHUNK_SIZE;

            ctx->chrm_int.white_point_x = read_u32(data);
            ctx->chrm_int.white_point_y = read_u32(data + 4);
            ctx->chrm_int.red_x = read_u32(data + 8);
            ctx->chrm_int.red_y = read_u32(data + 12);
            ctx->chrm_int.green_x = read_u32(data + 16);
            ctx->chrm_int.green_y = read_u32(data + 20);
            ctx->chrm_int.blue_x = read_u32(data + 24);
            ctx->chrm_int.blue_y = read_u32(data + 28);

            if(check_chrm_int(&ctx->chrm_int)) return SPNG_ECHRM;

            ctx->file_chrm = 1;
            ctx->stored_chrm = 1;
        }
        else if(!memcmp(chunk.type, type_gama, 4))
        {
            if(ctx->file_plte && chunk.offset > ctx->plte_offset) return SPNG_ECHUNK_POS;
            if(ctx->file_gama) return SPNG_EDUP_GAMA;

            if(chunk.length != 4) return SPNG_ECHUNK_SIZE;

            ctx->gama = read_u32(data);

            if(!ctx->gama) return SPNG_EGAMA;
            if(ctx->gama > png_u32max) return SPNG_EGAMA;

            ctx->file_gama = 1;
            ctx->stored_gama = 1;
        }
        else if(!memcmp(chunk.type, type_iccp, 4))
        {
            if(ctx->file_plte && chunk.offset > ctx->plte_offset) return SPNG_ECHUNK_POS;
            if(ctx->file_iccp) return SPNG_EDUP_ICCP;
            if(!chunk.length) return SPNG_ECHUNK_SIZE;

            continue; /* XXX: https://gitlab.com/randy408/libspng/issues/31 */
        }
        else if(!memcmp(chunk.type, type_sbit, 4))
        {
            if(ctx->file_plte && chunk.offset > ctx->plte_offset) return SPNG_ECHUNK_POS;
            if(ctx->file_sbit) return SPNG_EDUP_SBIT;

            if(ctx->ihdr.color_type == 0)
            {
                if(chunk.length != 1) return SPNG_ECHUNK_SIZE;

                memcpy(&ctx->sbit.grayscale_bits, data, 1);
            }
            else if(ctx->ihdr.color_type == 2 || ctx->ihdr.color_type == 3)
            {
                if(chunk.length != 3) return SPNG_ECHUNK_SIZE;

                memcpy(&ctx->sbit.red_bits, data, 1);
                memcpy(&ctx->sbit.green_bits, data + 1 , 1);
                memcpy(&ctx->sbit.blue_bits, data + 2, 1);
            }
            else if(ctx->ihdr.color_type == 4)
            {
                if(chunk.length != 2) return SPNG_ECHUNK_SIZE;

                memcpy(&ctx->sbit.grayscale_bits, data, 1);
                memcpy(&ctx->sbit.alpha_bits, data + 1, 1);
            }
            else if(ctx->ihdr.color_type == 6)
            {
                if(chunk.length != 4) return SPNG_ECHUNK_SIZE;

                memcpy(&ctx->sbit.red_bits, data, 1);
                memcpy(&ctx->sbit.green_bits, data + 1, 1);
                memcpy(&ctx->sbit.blue_bits, data + 2, 1);
                memcpy(&ctx->sbit.alpha_bits, data + 3, 1);
            }

            if(check_sbit(&ctx->sbit, &ctx->ihdr)) return SPNG_ESBIT;

            ctx->file_sbit = 1;
            ctx->stored_sbit = 1;
        }
        else if(!memcmp(chunk.type, type_srgb, 4))
        {
            if(ctx->file_plte && chunk.offset > ctx->plte_offset) return SPNG_ECHUNK_POS;
            if(ctx->file_srgb) return SPNG_EDUP_SRGB;

            if(chunk.length != 1) return SPNG_ECHUNK_SIZE;

            memcpy(&ctx->srgb_rendering_intent, data, 1);

            if(ctx->srgb_rendering_intent > 3) return SPNG_ESRGB;

            ctx->file_srgb = 1;
            ctx->stored_srgb = 1;
        }
        else if(!memcmp(chunk.type, type_bkgd, 4))
        {
            if(ctx->file_plte && chunk.offset < ctx->plte_offset) return SPNG_ECHUNK_POS;
            if(ctx->file_bkgd) return SPNG_EDUP_BKGD;

            uint16_t mask = ~0;
            if(ctx->ihdr.bit_depth < 16) mask = (1 << ctx->ihdr.bit_depth) - 1;

            if(ctx->ihdr.color_type == 0 || ctx->ihdr.color_type == 4)
            {
                if(chunk.length != 2) return SPNG_ECHUNK_SIZE;

                ctx->bkgd.gray = read_u16(data) & mask;
            }
            else if(ctx->ihdr.color_type == 2 || ctx->ihdr.color_type == 6)
            {
                if(chunk.length != 6) return SPNG_ECHUNK_SIZE;

                ctx->bkgd.red = read_u16(data) & mask;
                ctx->bkgd.green = read_u16(data + 2) & mask;
                ctx->bkgd.blue = read_u16(data + 4) & mask;
            }
            else if(ctx->ihdr.color_type == 3)
            {
                if(chunk.length != 1) return SPNG_ECHUNK_SIZE;
                if(!ctx->file_plte) return SPNG_EBKGD_NO_PLTE;

                memcpy(&ctx->bkgd.plte_index, data, 1);
                if(ctx->bkgd.plte_index >= ctx->plte.n_entries) return SPNG_EBKGD_PLTE_IDX;
            }

            ctx->file_bkgd = 1;
            ctx->stored_bkgd = 1;
        }
        else if(!memcmp(chunk.type, type_trns, 4))
        {
            if(ctx->file_plte && chunk.offset < ctx->plte_offset) return SPNG_ECHUNK_POS;
            if(ctx->file_trns) return SPNG_EDUP_TRNS;
            if(!chunk.length) return SPNG_ECHUNK_SIZE;

            uint16_t mask = ~0;
            if(ctx->ihdr.bit_depth < 16) mask = (1 << ctx->ihdr.bit_depth) - 1;

            if(ctx->ihdr.color_type == 0)
            {
                if(chunk.length != 2) return SPNG_ECHUNK_SIZE;

                ctx->trns.gray = read_u16(data) & mask;
            }
            else if(ctx->ihdr.color_type == 2)
            {
                if(chunk.length != 6) return SPNG_ECHUNK_SIZE;

                ctx->trns.red = read_u16(data) & mask;
                ctx->trns.green = read_u16(data + 2) & mask;
                ctx->trns.blue = read_u16(data + 4) & mask;
            }
            else if(ctx->ihdr.color_type == 3)
            {
                if(chunk.length > ctx->plte.n_entries) return SPNG_ECHUNK_SIZE;
                if(!ctx->file_plte) return SPNG_ETRNS_NO_PLTE;

                size_t k;
                for(k=0; k < chunk.length; k++)
                {
                    memcpy(&ctx->trns.type3_alpha[k], data + k, 1);
                }
                ctx->trns.n_type3_entries = chunk.length;
            }
            else return SPNG_ETRNS_COLOR_TYPE;

            ctx->file_trns = 1;
            ctx->stored_trns = 1;
        }
        else if(!memcmp(chunk.type, type_hist, 4))
        {
            if(!ctx->file_plte) return SPNG_EHIST_NO_PLTE;
            if(chunk.offset < ctx->plte_offset) return SPNG_ECHUNK_POS;
            if(ctx->file_hist) return SPNG_EDUP_HIST;

            if( (chunk.length / 2) != (ctx->plte.n_entries) ) return SPNG_ECHUNK_SIZE;

            size_t k;
            for(k=0; k < (chunk.length / 2); k++)
            {
                ctx->hist.frequency[k] = read_u16(data + k*2);
            }

            ctx->file_hist = 1;
            ctx->stored_hist = 1;
        }
        else if(!memcmp(chunk.type, type_phys, 4))
        {
            if(ctx->file_phys) return SPNG_EDUP_PHYS;

            if(chunk.length != 9) return SPNG_ECHUNK_SIZE;

            ctx->phys.ppu_x = read_u32(data);
            ctx->phys.ppu_y = read_u32(data + 4);
            memcpy(&ctx->phys.unit_specifier, data + 8, 1);

            if(check_phys(&ctx->phys)) return SPNG_EPHYS;

            ctx->file_phys = 1;
            ctx->stored_phys = 1;
        }
        else if(!memcmp(chunk.type, type_splt, 4))
        {
            if(ctx->user_splt) continue; /* XXX: should check profile names for uniqueness */
            if(!chunk.length) return SPNG_ECHUNK_SIZE;

            ctx->file_splt = 1;

            if(!ctx->stored_splt)
            {
                ctx->n_splt = 1;
                ctx->splt_list = spng__calloc(ctx, 1, sizeof(struct spng_splt));
                if(ctx->splt_list == NULL) return SPNG_EMEM;
            }
            else
            {
                ctx->n_splt++;
                if(ctx->n_splt < 1) return SPNG_EOVERFLOW;
                if(sizeof(struct spng_splt) > SIZE_MAX / ctx->n_splt) return SPNG_EOVERFLOW;

                void *buf = spng__realloc(ctx, ctx->splt_list, ctx->n_splt * sizeof(struct spng_splt));
                if(buf == NULL) return SPNG_EMEM;
                ctx->splt_list = buf;
                memset(&ctx->splt_list[ctx->n_splt - 1], 0, sizeof(struct spng_splt));
            }

            uint32_t i = ctx->n_splt - 1;

            size_t keyword_len = chunk.length > 80 ? 80 : chunk.length;
            char *keyword_nul = memchr(data, '\0', keyword_len);
            if(keyword_nul == NULL) return SPNG_ESPLT_NAME;

            memcpy(&ctx->splt_list[i].name, data, keyword_len);

            if(check_png_keyword(ctx->splt_list[i].name)) return SPNG_ESPLT_NAME;

            keyword_len = strlen(ctx->splt_list[i].name);

            if( (chunk.length - keyword_len - 1) ==  0) return SPNG_ECHUNK_SIZE;

            memcpy(&ctx->splt_list[i].sample_depth, data + keyword_len + 1, 1);

            if(ctx->n_splt > 1)
            {
                uint32_t j;
                for(j=0; j < i; j++)
                {
                    if(!strcmp(ctx->splt_list[j].name, ctx->splt_list[i].name)) return SPNG_ESPLT_DUP_NAME;
                }
            }

            if(ctx->splt_list[i].sample_depth == 16)
            {
                if( (chunk.length - keyword_len - 2) % 10 != 0) return SPNG_ECHUNK_SIZE;
                ctx->splt_list[i].n_entries = (chunk.length - keyword_len - 2) / 10;
            }
            else if(ctx->splt_list[i].sample_depth == 8)
            {
                if( (chunk.length - keyword_len - 2) % 6 != 0) return SPNG_ECHUNK_SIZE;
                ctx->splt_list[i].n_entries = (chunk.length - keyword_len - 2) / 6;
            }
            else return SPNG_ESPLT_DEPTH;

            if(ctx->splt_list[i].n_entries == 0) return SPNG_ECHUNK_SIZE;
            if(sizeof(struct spng_splt_entry) > SIZE_MAX / ctx->splt_list[i].n_entries) return SPNG_EOVERFLOW;

            ctx->splt_list[i].entries = spng__malloc(ctx, sizeof(struct spng_splt_entry) * ctx->splt_list[i].n_entries);
            if(ctx->splt_list[i].entries == NULL) return SPNG_EMEM;

            const unsigned char *splt = data + keyword_len + 2;

            size_t k;
            if(ctx->splt_list[i].sample_depth == 16)
            {
                for(k=0; k < ctx->splt_list[i].n_entries; k++)
                {
                    ctx->splt_list[i].entries[k].red = read_u16(splt + k * 10);
                    ctx->splt_list[i].entries[k].green = read_u16(splt + k * 10 + 2);
                    ctx->splt_list[i].entries[k].blue = read_u16(splt + k * 10 + 4);
                    ctx->splt_list[i].entries[k].alpha = read_u16(splt + k * 10 + 6);
                    ctx->splt_list[i].entries[k].frequency = read_u16(splt + k * 10 + 8);
                }
            }
            else if(ctx->splt_list[i].sample_depth == 8)
            {
                for(k=0; k < ctx->splt_list[i].n_entries; k++)
                {
                    uint8_t red, green, blue, alpha;
                    memcpy(&red,   splt + k * 6, 1);
                    memcpy(&green, splt + k * 6 + 1, 1);
                    memcpy(&blue,  splt + k * 6 + 2, 1);
                    memcpy(&alpha, splt + k * 6 + 3, 1);
                    ctx->splt_list[i].entries[k].frequency = read_u16(splt + k * 6 + 4);

                    ctx->splt_list[i].entries[k].red = red;
                    ctx->splt_list[i].entries[k].green = green;
                    ctx->splt_list[i].entries[k].blue = blue;
                    ctx->splt_list[i].entries[k].alpha = alpha;
                }
            }

            ctx->stored_splt = 1;
        }
        else if(!memcmp(chunk.type, type_time, 4))
        {
            if(ctx->file_time) return SPNG_EDUP_TIME;

            if(chunk.length != 7) return SPNG_ECHUNK_SIZE;

            struct spng_time time;

            time.year = read_u16(data);
            memcpy(&time.month, data + 2, 1);
            memcpy(&time.day, data + 3, 1);
            memcpy(&time.hour, data + 4, 1);
            memcpy(&time.minute, data + 5, 1);
            memcpy(&time.second, data + 6, 1);

            if(check_time(&time)) return SPNG_ETIME;

            ctx->file_time = 1;

            if(!ctx->user_time) memcpy(&ctx->time, &time, sizeof(struct spng_time));

            ctx->stored_time = 1;
        }
        else if(!memcmp(chunk.type, type_text, 4) ||
                !memcmp(chunk.type, type_ztxt, 4) ||
                !memcmp(chunk.type, type_itxt, 4))
        {
            ctx->file_text = 1;

            continue; /* XXX: https://gitlab.com/randy408/libspng/issues/31 */
        }
        else if(!memcmp(chunk.type, type_offs, 4))
        {
            if(ctx->file_offs) return SPNG_EDUP_OFFS;

            if(chunk.length != 9) return SPNG_ECHUNK_SIZE;

            ctx->offs.x = read_s32(data);
            ctx->offs.y = read_s32(data + 4);
            memcpy(&ctx->offs.unit_specifier, data + 8, 1);

            if(check_offs(&ctx->offs)) return SPNG_EOFFS;

            ctx->file_offs = 1;
            ctx->stored_offs = 1;
        }
        else if(!memcmp(chunk.type, type_exif, 4))
        {
            if(ctx->file_exif) return SPNG_EDUP_EXIF;

            ctx->file_exif = 1;

            struct spng_exif exif;

            exif.data = spng__malloc(ctx, chunk.length);
            if(exif.data == NULL) return SPNG_EMEM;

            memcpy(exif.data, data, chunk.length);
            exif.length = chunk.length;

            if(check_exif(&exif))
            {
                spng__free(ctx, exif.data);
                return SPNG_EEXIF;
            }

            if(!ctx->user_exif) memcpy(&ctx->exif, &exif, sizeof(struct spng_exif));
            else spng__free(ctx, exif.data);

            ctx->stored_exif = 1;
        }
    }

    return ret;
}

static int validate_past_idat(spng_ctx *ctx)
{
    if(ctx == NULL) return 1;

    int ret;
    int prev_was_idat = 1;
    struct spng_chunk chunk;
    unsigned char *data;

    memcpy(&chunk, &ctx->last_idat, sizeof(struct spng_chunk));

    while( !(ret = read_header(ctx)))
    {
        memcpy(&chunk, &ctx->current_chunk, sizeof(struct spng_chunk));

        ret = read_chunk_bytes(ctx, chunk.length);
        if(ret) return ret;

        data = ctx->data;

        /* Reserved bit must be zero */
        if( (chunk.type[2] & (1 << 5)) != 0) return SPNG_ECHUNK_TYPE;
         /* Ignore private chunks */
        if( (chunk.type[1] & (1 << 5)) != 0) continue;

        /* Critical chunk */
        if(is_critical_chunk(&chunk))
        {
            if(!memcmp(chunk.type, type_iend, 4)) return 0;
            else if(!memcmp(chunk.type, type_idat, 4) && prev_was_idat) continue; /* ignore extra IDATs */
            else return SPNG_ECHUNK_POS; /* critical chunk after last IDAT that isn't IEND */
        }

        prev_was_idat = 0;

        if(!memcmp(chunk.type, type_chrm, 4)) return SPNG_ECHUNK_POS;
        else if(!memcmp(chunk.type, type_gama, 4)) return SPNG_ECHUNK_POS;
        else if(!memcmp(chunk.type, type_iccp, 4)) return SPNG_ECHUNK_POS;
        else if(!memcmp(chunk.type, type_sbit, 4)) return SPNG_ECHUNK_POS;
        else if(!memcmp(chunk.type, type_srgb, 4)) return SPNG_ECHUNK_POS;
        else if(!memcmp(chunk.type, type_bkgd, 4)) return SPNG_ECHUNK_POS;
        else if(!memcmp(chunk.type, type_hist, 4)) return SPNG_ECHUNK_POS;
        else if(!memcmp(chunk.type, type_trns, 4)) return SPNG_ECHUNK_POS;
        else if(!memcmp(chunk.type, type_phys, 4)) return SPNG_ECHUNK_POS;
        else if(!memcmp(chunk.type, type_splt, 4)) return SPNG_ECHUNK_POS;
        else if(!memcmp(chunk.type, type_offs, 4)) return SPNG_ECHUNK_POS;
        else if(!memcmp(chunk.type, type_time, 4))
        {
           if(ctx->file_time) return SPNG_EDUP_TIME;

           if(chunk.length != 7) return SPNG_ECHUNK_SIZE;

            struct spng_time time;

            time.year = read_u16(data);
            memcpy(&time.month, data + 2, 1);
            memcpy(&time.day, data + 3, 1);
            memcpy(&time.hour, data + 4, 1);
            memcpy(&time.minute, data + 5, 1);
            memcpy(&time.second, data + 6, 1);

            if(check_time(&time)) return SPNG_ETIME;

            ctx->file_time = 1;

            if(!ctx->user_time) memcpy(&ctx->time, &time, sizeof(struct spng_time));

            ctx->stored_time = 1;
        }
        else if(!memcmp(chunk.type, type_exif, 4))
        {
            if(ctx->file_exif) return SPNG_EDUP_EXIF;

            ctx->file_exif = 1;

            struct spng_exif exif;

            exif.data = spng__malloc(ctx, chunk.length);
            if(exif.data == NULL) return SPNG_EMEM;

            memcpy(exif.data, data, chunk.length);
            exif.length = chunk.length;

            if(check_exif(&exif))
            {
                spng__free(ctx, exif.data);
                return SPNG_EEXIF;
            }

            if(!ctx->user_exif) memcpy(&ctx->exif, &exif, sizeof(struct spng_exif));
            else spng__free(ctx, exif.data);

            ctx->stored_exif = 1;
        }
        else if(!memcmp(chunk.type, type_text, 4) ||
                !memcmp(chunk.type, type_ztxt, 4) ||
                !memcmp(chunk.type, type_itxt, 4))
        {
            ctx->file_text = 1;

            continue; /* XXX: https://gitlab.com/randy408/libspng/issues/31 */
        }
    }

    return ret;
}


/* Scale "sbits" significant bits in "sample" from "bit_depth" to "target"

   "bit_depth" must be a valid PNG depth
   "sbits" must be less than or equal to "bit_depth"
   "target" must be between 1 and 16
*/
static uint16_t sample_to_target(uint16_t sample, unsigned bit_depth, unsigned sbits, unsigned target)
{
    if(bit_depth == sbits)
    {
        if(target == sbits) return sample; /* no scaling */
    }/* bit_depth > sbits */
    else sample = sample >> (bit_depth - sbits); /* shift significant bits to bottom */

    /* downscale */
    if(target < sbits) return sample >> (sbits - target);

    /* upscale using left bit replication */
    int8_t shift_amount = target - sbits;
    uint16_t sample_bits = sample;
    sample = 0;

    while(shift_amount >= 0)
    {
        sample = sample | (sample_bits << shift_amount);
        shift_amount -= sbits;
    }

    int8_t partial = shift_amount + (int8_t)sbits;

    if(partial != 0) sample = sample | (sample_bits >> abs(shift_amount));

    return sample;
}

int get_ancillary(spng_ctx *ctx)
{
    if(ctx == NULL) return 1;
    if(ctx->data == NULL) return 1;
    if(!ctx->valid_state) return SPNG_EBADSTATE;

    int ret;
    if(!ctx->first_idat.offset)
    {
        ret = get_ancillary_data_first_idat(ctx);
        if(ret)
        {
            ctx->valid_state = 0;
            return ret;
        }
    }

    return 0;
}

/* Same as above except it returns 0 if no buffer is set */
int get_ancillary2(spng_ctx *ctx)
{
    if(ctx == NULL) return 1;
    if(!ctx->valid_state) return SPNG_EBADSTATE;

    if(ctx->data == NULL) return 0;

    int ret;
    if(!ctx->first_idat.offset)
    {
        ret = get_ancillary_data_first_idat(ctx);
        if(ret)
        {
            ctx->valid_state = 0;
            return ret;
        }
    }

    return 0;
}

int spng_decode_image(spng_ctx *ctx, void *out, size_t out_size, int fmt, int flags)
{
    if(ctx == NULL) return 1;
    if(out == NULL) return 1;
    if(!ctx->valid_state) return SPNG_EBADSTATE;
    if(ctx->encode_only) return SPNG_ENCODE_ONLY;

    int ret;
    size_t out_size_required;

    ret = spng_decoded_image_size(ctx, fmt, &out_size_required);
    if(ret) return ret;
    if(out_size < out_size_required) return SPNG_EBUFSIZ;

    ret = get_ancillary(ctx);
    if(ret) return ret;

    uint8_t channels = 1; /* grayscale or indexed_color */

    if(ctx->ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR) channels = 3;
    else if(ctx->ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE_ALPHA) channels = 2;
    else if(ctx->ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR_ALPHA) channels = 4;

    uint8_t bytes_per_pixel;

    if(ctx->ihdr.bit_depth < 8) bytes_per_pixel = 1;
    else bytes_per_pixel = channels * (ctx->ihdr.bit_depth / 8);

    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    if(inflateInit(&stream) != Z_OK) return SPNG_EZLIB;

    struct spng_subimage sub[7];
    memset(sub, 0, sizeof(struct spng_subimage) * 7);

    size_t scanline_width;

    ret = calculate_subimages(sub, &scanline_width, &ctx->ihdr, channels);
    if(ret) return ret;

    unsigned char *scanline = NULL, *prev_scanline = NULL;

    scanline = spng__malloc(ctx, scanline_width);
    prev_scanline = spng__malloc(ctx, scanline_width);

    if(scanline == NULL || prev_scanline == NULL)
    {
        ret = SPNG_EMEM;
        goto decode_err;
    }

    int i;
    for(i=0; i < 7; i++)
    {
        /* Skip empty passes */
        if(sub[i].width != 0 && sub[i].height != 0)
        {
            scanline_width = sub[i].scanline_width;
            break;
        }
    }

    uint16_t *gamma_lut = NULL;
    uint16_t gamma_lut8[256];

    if(flags & SPNG_DECODE_USE_GAMA && ctx->stored_gama)
    {
        float file_gamma = (float)ctx->gama / 100000.0f;
        float max;

        uint32_t i, lut_entries;

        if(fmt == SPNG_FMT_RGBA8)
        {
            lut_entries = 256;
            max = 255.0f;
            
            gamma_lut = gamma_lut8;
        }
        else /* SPNG_FMT_RGBA16 */
        {
            lut_entries = 65536;
            max = 65535.0f;

            ctx->gamma_lut = spng__malloc(ctx, lut_entries * sizeof(uint16_t));
            if(ctx->gamma_lut == NULL)
            {
                ret = SPNG_EMEM;
                goto decode_err;
            }
            gamma_lut = ctx->gamma_lut;
        }
        
        float screen_gamma = 2.2f;
        float exponent = file_gamma * screen_gamma;

        if(FP_ZERO == fpclassify(exponent))
        {
            ret = SPNG_EGAMA;
            goto decode_err;
        }

        exponent = 1.0f / exponent;
        
        for(i=0; i < lut_entries; i++)
        {
            float c = pow((float)i / max, exponent) * max;
            c = fmin(c, max);

            gamma_lut[i] = c;
        }
    }

    unsigned red_sbits, green_sbits, blue_sbits, alpha_sbits, grayscale_sbits;

    red_sbits = ctx->ihdr.bit_depth;
    green_sbits = ctx->ihdr.bit_depth;
    blue_sbits = ctx->ihdr.bit_depth;
    alpha_sbits = ctx->ihdr.bit_depth;
    grayscale_sbits = ctx->ihdr.bit_depth;

    if(ctx->ihdr.color_type == SPNG_COLOR_TYPE_INDEXED)
    {
        red_sbits = 8;
        green_sbits = 8;
        blue_sbits = 8;
        alpha_sbits = 8;
    }

    if(ctx->stored_sbit)
    {
        if(flags & SPNG_DECODE_USE_SBIT)
        {
            if(ctx->ihdr.color_type == 0)
            {
                grayscale_sbits = ctx->sbit.grayscale_bits;
                alpha_sbits = ctx->ihdr.bit_depth;
            }
            else if(ctx->ihdr.color_type == 2 || ctx->ihdr.color_type == 3)
            {
                red_sbits = ctx->sbit.red_bits;
                green_sbits = ctx->sbit.green_bits;
                blue_sbits = ctx->sbit.blue_bits;
                alpha_sbits = ctx->ihdr.bit_depth;
            }
            else if(ctx->ihdr.color_type == 4)
            {
                grayscale_sbits = ctx->sbit.grayscale_bits;
                alpha_sbits = ctx->sbit.alpha_bits;
            }
            else /* == 6 */
            {
                red_sbits = ctx->sbit.red_bits;
                green_sbits = ctx->sbit.green_bits;
                blue_sbits = ctx->sbit.blue_bits;
                alpha_sbits = ctx->sbit.alpha_bits;
            }
        }
    }

    /* Calculate the palette's alpha channel ahead of time */
    if(ctx->ihdr.color_type == SPNG_COLOR_TYPE_INDEXED)
    {
        for(i=0; i < ctx->plte.n_entries; i++)
        {
            if(flags & SPNG_DECODE_USE_TRNS && ctx->stored_trns && i < ctx->trns.n_type3_entries)
                ctx->plte.entries[i].alpha = ctx->trns.type3_alpha[i];
            else
                ctx->plte.entries[i].alpha = 255;
        }
    }

    unsigned depth_target = 8; /* FMT_RGBA8 */
    if(fmt == SPNG_FMT_RGBA16) depth_target = 16;

    uint32_t bytes_read;

    ret = get_idat_bytes(ctx, &bytes_read);
    if(ret) goto decode_err;

    stream.avail_in = bytes_read;
    stream.next_in = ctx->data;

    int pass;
    uint8_t filter = 0, next_filter = 0;
    uint32_t scanline_idx;

    uint32_t k;
    uint8_t r_8, g_8, b_8, a_8, gray_8;
    uint16_t r_16, g_16, b_16, a_16, gray_16;
    uint16_t r, g, b, a, gray;
    unsigned char pixel[8] = {0};
    size_t pixel_offset = 0;
    size_t pixel_size = 4; /* SPNG_FMT_RGBA8 */
    unsigned processing_depth = ctx->ihdr.bit_depth;
                
    if(fmt == SPNG_FMT_RGBA16) pixel_size = 8;

    if(ctx->ihdr.color_type == SPNG_COLOR_TYPE_INDEXED) processing_depth = 8;

    for(pass=0; pass < 7; pass++)
    {
        /* Skip empty passes */
        if(sub[pass].width == 0 || sub[pass].height == 0) continue;

        scanline_width = sub[pass].scanline_width;

        /* prev_scanline is all zeros for the first scanline */
        memset(prev_scanline, 0, scanline_width);

        /* Read the first filter byte, offsetting all reads by 1 byte.
           The scanlines will be aligned with the start of the array with
           the next scanline's filter byte at the end,
           the last scanline will end up being 1 byte "shorter". */
        stream.avail_out = 1;
        stream.next_out = &filter;

        do
        {
            ret = inflate(&stream, Z_SYNC_FLUSH);

            if(ret != Z_OK)
            {
                if(ret == Z_STREAM_END)
                {
                    if(stream.avail_out !=0)
                    {
                        ret = SPNG_EIDAT_TOO_SHORT;
                        goto decode_err;
                    }
                }
                else if(ret != Z_BUF_ERROR)
                {
                    ret = SPNG_EIDAT_STREAM;
                    goto decode_err;
                }
            }

            if(stream.avail_out != 0 && stream.avail_in == 0)
            {
                ret = get_idat_bytes(ctx, &bytes_read);
                if(ret) goto decode_err;

                stream.avail_in = bytes_read;
                stream.next_in = ctx->data;
            }

        }while(stream.avail_out != 0);


        for(scanline_idx=0; scanline_idx < sub[pass].height; scanline_idx++)
        {
            stream.next_out = scanline;

            /* The last scanline is 1 byte "shorter" */
            if(scanline_idx == (sub[pass].height - 1)) stream.avail_out = scanline_width - 1;
            else stream.avail_out = scanline_width;

            do
            {
                ret = inflate(&stream, Z_SYNC_FLUSH);

                if(ret != Z_OK)
                {
                    if(ret == Z_STREAM_END) /* zlib reached an end-marker */
                    {/* we don't have a full scanline or there are more scanlines left */
                        if(stream.avail_out != 0 || scanline_idx != (sub[pass].height - 1))
                        {
                            ret = SPNG_EIDAT_TOO_SHORT;
                            goto decode_err;
                        }
                    }
                    else if(ret != Z_BUF_ERROR)
                    {
                        ret = SPNG_EIDAT_STREAM;
                        goto decode_err;
                    }
                }

                /* We don't have scanline_width of data and we ran out of IDAT bytes */
                if(stream.avail_out != 0 && stream.avail_in == 0)
                {
                    ret = get_idat_bytes(ctx, &bytes_read);
                    if(ret) goto decode_err;

                    stream.avail_in = bytes_read;
                    stream.next_in = ctx->data;
                }

            }while(stream.avail_out != 0);

            memcpy(&next_filter, scanline + scanline_width - 1, 1);

            ret = defilter_scanline(prev_scanline, scanline, scanline_width - 1, bytes_per_pixel, filter);
            if(ret) goto decode_err;

            filter = next_filter;

            r=0; g=0; b=0; a=0; gray=0;
            r_8=0; g_8=0; b_8=0; a_8=0; gray_8=0;
            r_16=0; g_16=0; b_16=0; a_16=0; gray_16=0;

            /* Process a scanline per-pixel and write to *out */
            for(k=0; k < sub[pass].width; k++)
            {
                /* Extract a pixel from the scanline,
                   *_16/8 variables are used for memcpy'ing depending on bit_depth,
                   r, g, b, a, gray (all 16bits) are used for processing */
                switch(ctx->ihdr.color_type)
                {
                    case SPNG_COLOR_TYPE_GRAYSCALE:
                    {
                        if(ctx->ihdr.bit_depth == 16)
                        {
                            gray_16 = read_u16(scanline + (k * 2));

                            if(flags & SPNG_DECODE_USE_TRNS && ctx->stored_trns &&
                               ctx->trns.gray == gray_16) a_16 = 0;
                            else a_16 = 65535;
                        }
                        else /* <= 8 */
                        {
                            memcpy(&gray_8, scanline + k / (8 / ctx->ihdr.bit_depth), 1);

                            uint16_t mask16 = (1 << ctx->ihdr.bit_depth) - 1;
                            uint8_t mask = mask16; /* avoid shift by width */
                            uint8_t samples_per_byte = 8 / ctx->ihdr.bit_depth;
                            uint8_t max_shift_amount = 8 - ctx->ihdr.bit_depth;
                            uint8_t shift_amount = max_shift_amount - ((k % samples_per_byte) * ctx->ihdr.bit_depth);

                            gray_8 = gray_8 & (mask << shift_amount);
                            gray_8 = gray_8 >> shift_amount;

                            if(flags & SPNG_DECODE_USE_TRNS && ctx->stored_trns &&
                               ctx->trns.gray == gray_8) a_8 = 0;
                            else a_8 = 255;
                        }

                        break;
                    }
                    case SPNG_COLOR_TYPE_TRUECOLOR:
                    {
                        if(ctx->ihdr.bit_depth == 16)
                        {
                            r_16 = read_u16(scanline + (k * 6));
                            g_16 = read_u16(scanline + (k * 6) + 2);
                            b_16 = read_u16(scanline + (k * 6) + 4);

                            if(flags & SPNG_DECODE_USE_TRNS && ctx->stored_trns &&
                               ctx->trns.red == r_16 &&
                               ctx->trns.green == g_16 &&
                               ctx->trns.blue == b_16) a_16 = 0;
                            else a_16 = 65535;
                        }
                        else /* == 8 */
                        {
                            memcpy(&r_8, scanline + (k * 3), 1);
                            memcpy(&g_8, scanline + (k * 3) + 1, 1);
                            memcpy(&b_8, scanline + (k * 3) + 2, 1);

                            if(flags & SPNG_DECODE_USE_TRNS && ctx->stored_trns &&
                               ctx->trns.red == r_8 &&
                               ctx->trns.green == g_8 &&
                               ctx->trns.blue == b_8) a_8 = 0;
                            else a_8 = 255;
                        }

                        break;
                    }
                    case SPNG_COLOR_TYPE_INDEXED:
                    {
                        uint8_t entry = 0;

                        if(ctx->ihdr.bit_depth == 8)
                        {
                            memcpy(&entry, scanline + k, 1);
                        }
                        else
                        {
                            memcpy(&entry, scanline + k / (8 / ctx->ihdr.bit_depth), 1);

                            uint8_t mask = (1 << ctx->ihdr.bit_depth) - 1;
                            uint8_t samples_per_byte = 8 / ctx->ihdr.bit_depth;
                            uint8_t max_shift_amount = 8 - ctx->ihdr.bit_depth;
                            uint8_t shift_amount = max_shift_amount - ((k % samples_per_byte) * ctx->ihdr.bit_depth);

                            entry = entry & (mask << shift_amount);
                            entry = entry >> shift_amount;
                        }

                        if(entry >= ctx->plte.n_entries)
                        {
                            ret = SPNG_EPLTE_IDX;
                            goto decode_err;
                        }
                      
                        r_8 = ctx->plte.entries[entry].red;
                        g_8 = ctx->plte.entries[entry].green;
                        b_8 = ctx->plte.entries[entry].blue;
                        a_8 = ctx->plte.entries[entry].alpha;
                
                        break;
                    }
                    case SPNG_COLOR_TYPE_GRAYSCALE_ALPHA:
                    {
                        if(ctx->ihdr.bit_depth == 16)
                        {
                            gray_16 = read_u16(scanline + (k * 4));
                            a_16 = read_u16(scanline + (k * 4) + 2);
                        }
                        else /* == 8 */
                        {
                            memcpy(&gray_8, scanline + (k * 2), 1);
                            memcpy(&a_8, scanline + (k * 2) + 1, 1);
                        }

                        break;
                    }
                    case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA:
                    {
                        if(ctx->ihdr.bit_depth == 16)
                        {
                            r_16 = read_u16(scanline + (k * 8));
                            g_16 = read_u16(scanline + (k * 8) + 2);
                            b_16 = read_u16(scanline + (k * 8) + 4);
                            a_16 = read_u16(scanline + (k * 8) + 6);
                        }
                        else /* == 8 */
                        {
                            memcpy(&r_8, scanline + (k * 4), 1);
                            memcpy(&g_8, scanline + (k * 4) + 1, 1);
                            memcpy(&b_8, scanline + (k * 4) + 2, 1);
                            memcpy(&a_8, scanline + (k * 4) + 3, 1);
                        }

                        break;
                    }
                }/* switch(ctx->ihdr.color_type) */


                if(ctx->ihdr.bit_depth == 16)
                {
                    r = r_16; g = g_16; b = b_16; a = a_16;
                    gray = gray_16;
                }
                else
                {
                    r = r_8; g = g_8; b = b_8; a = a_8;
                    gray = gray_8;
                }


                if(ctx->ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE ||
                   ctx->ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE_ALPHA)
                {
                    gray = sample_to_target(gray, ctx->ihdr.bit_depth, grayscale_sbits, depth_target);
                    a = sample_to_target(a, ctx->ihdr.bit_depth, alpha_sbits, depth_target);
                }
                else
                {
                    r = sample_to_target(r, processing_depth, red_sbits, depth_target);
                    g = sample_to_target(g, processing_depth, green_sbits, depth_target);
                    b = sample_to_target(b, processing_depth, blue_sbits, depth_target);
                    a = sample_to_target(a, processing_depth, alpha_sbits, depth_target);
                }


                if(ctx->ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE ||
                   ctx->ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE_ALPHA)
                {
                    r = gray;
                    g = gray;
                    b = gray;
                }


                if(flags & SPNG_DECODE_USE_GAMA && ctx->stored_gama)
                {
                    r = gamma_lut[r];
                    g = gamma_lut[g];
                    b = gamma_lut[b];
                }

                /* only use *_8/16 for memcpy */
                r_8 = r; g_8 = g; b_8 = b; a_8 = a;
                r_16 = r; g_16 = g; b_16 = b; a_16 = a;
                gray_8 = gray;
                gray_16 = gray;

                if(fmt == SPNG_FMT_RGBA8)
                {
                    memcpy(pixel, &r_8, 1);
                    memcpy(pixel + 1, &g_8, 1);
                    memcpy(pixel + 2, &b_8, 1);
                    memcpy(pixel + 3, &a_8, 1);
                }
                else if(fmt == SPNG_FMT_RGBA16)
                {
                    memcpy(pixel, &r_16, 2);
                    memcpy(pixel + 2, &g_16, 2);
                    memcpy(pixel + 4, &b_16, 2);
                    memcpy(pixel + 6, &a_16, 2);
                }

                if(!ctx->ihdr.interlace_method)
                {
                    memcpy((char*)out + pixel_offset, pixel, pixel_size);

                    pixel_offset = pixel_offset + pixel_size;
                }
                else
                {
                    const unsigned int adam7_x_start[7] = { 0, 4, 0, 2, 0, 1, 0 };
                    const unsigned int adam7_y_start[7] = { 0, 0, 4, 0, 2, 0, 1 };
                    const unsigned int adam7_x_delta[7] = { 8, 8, 4, 4, 2, 2, 1 };
                    const unsigned int adam7_y_delta[7] = { 8, 8, 8, 4, 4, 2, 2 };

                    pixel_offset = ((adam7_y_start[pass] + scanline_idx * adam7_y_delta[pass]) *
                                     ctx->ihdr.width + adam7_x_start[pass] + k * adam7_x_delta[pass]) * pixel_size;

                    memcpy((char*)out + pixel_offset, pixel, pixel_size);
                }
                
            }/* for(k=0; k < sub[pass].width; k++) */

            /* NOTE: prev_scanline is always defiltered */
            memcpy(prev_scanline, scanline, scanline_width);

        }/* for(scanline_idx=0; scanline_idx < sub[pass].height; scanline_idx++) */
    }/* for(pass=0; pass < 7; pass++) */

    if(ctx->cur_chunk_bytes_left) /* zlib stream ended before an IDAT chunk boundary */
    {/* discard the rest of the chunk */
        ret = discard_chunk_bytes(ctx, ctx->cur_chunk_bytes_left);
    }

decode_err:

    inflateEnd(&stream);
    spng__free(ctx, scanline);
    spng__free(ctx, prev_scanline);

    if(ret)
    {
        ctx->valid_state = 0;
        return ret;
    }

    memcpy(&ctx->last_idat, &ctx->current_chunk, sizeof(struct spng_chunk));

    ret = validate_past_idat(ctx);

    if(ret) ctx->valid_state = 0;

    return ret;
}

inline void *spng__malloc(spng_ctx *ctx,  size_t size)
{
    return ctx->alloc.malloc_fn(size);
}

inline void *spng__calloc(spng_ctx *ctx, size_t nmemb, size_t size)
{
    return ctx->alloc.calloc_fn(nmemb, size);
}

inline void *spng__realloc(spng_ctx *ctx, void *ptr, size_t size)
{
    return ctx->alloc.realloc_fn(ptr, size);
}

inline void spng__free(spng_ctx *ctx, void *ptr)
{
    ctx->alloc.free_fn(ptr);
}


spng_ctx *spng_ctx_new(int flags)
{
    if(flags) return NULL;

    struct spng_alloc alloc = {0};

    alloc.malloc_fn = malloc;
    alloc.realloc_fn = realloc;
    alloc.calloc_fn = calloc;
    alloc.free_fn = free;

    return spng_ctx_new2(&alloc, flags);
}

spng_ctx *spng_ctx_new2(struct spng_alloc *alloc, int flags)
{
    if(alloc == NULL) return NULL;
    if(flags) return NULL;

    if(alloc->malloc_fn == NULL) return NULL;
    if(alloc->realloc_fn == NULL) return NULL;
    if(alloc->calloc_fn == NULL) return NULL;
    if(alloc->free_fn == NULL) return NULL;

    spng_ctx *ctx = alloc->calloc_fn(1, sizeof(spng_ctx));
    if(ctx == NULL) return NULL;
    
    memcpy(&ctx->alloc, alloc, sizeof(struct spng_alloc));

    ctx->max_chunk_size = png_u32max;
    ctx->chunk_cache_limit = SIZE_MAX;

    ctx->valid_state = 1;

    return ctx;
}

void spng_ctx_free(spng_ctx *ctx)
{
    if(ctx == NULL) return;

    if(ctx->streaming && ctx->data != NULL) spng__free(ctx, ctx->data);

    if(ctx->exif.data != NULL && !ctx->user_exif) spng__free(ctx, ctx->exif.data);

    if(ctx->iccp.profile != NULL && !ctx->user_iccp) spng__free(ctx, ctx->iccp.profile);

    if(ctx->gamma_lut != NULL) spng__free(ctx, ctx->gamma_lut);

    if(ctx->splt_list != NULL && !ctx->user_splt)
    {
        uint32_t i;
        for(i=0; i < ctx->n_splt; i++)
        {
            if(ctx->splt_list[i].entries != NULL) spng__free(ctx, ctx->splt_list[i].entries);
        }
        spng__free(ctx, ctx->splt_list);
    }

    if(ctx->text_list != NULL && !ctx->user_text)
    {
        uint32_t i;
        for(i=0; i< ctx->n_text; i++)
        {
            if(ctx->text_list[i].text != NULL) spng__free(ctx, ctx->text_list[i].text);
            if(ctx->text_list[i].language_tag != NULL) spng__free(ctx, ctx->text_list[i].language_tag);
            if(ctx->text_list[i].translated_keyword != NULL) spng__free(ctx, ctx->text_list[i].translated_keyword);
        }
        spng__free(ctx, ctx->text_list);
    }

    spng_free_fn *free_func = ctx->alloc.free_fn;

    memset(ctx, 0, sizeof(spng_ctx));

    free_func(ctx);
}

static int buffer_read_fn(spng_ctx *ctx, void *user, void *data, size_t n)
{
    if(n > ctx->bytes_left) return SPNG_IO_EOF;

    ctx->data = ctx->data + ctx->last_read_size;

    ctx->last_read_size = n;
    ctx->bytes_left -= n;

    return 0;
}

int spng_set_png_buffer(spng_ctx *ctx, void *buf, size_t size)
{
    if(ctx == NULL || buf == NULL) return 1;
    if(!ctx->valid_state) return SPNG_EBADSTATE;

    if(ctx->data != NULL) return SPNG_EBUF_SET;

    ctx->data = buf;
    ctx->png_buf = buf;
    ctx->data_size = size;
    ctx->bytes_left = size;

    ctx->read_fn = buffer_read_fn;

    return 0;
}

int spng_set_png_stream(spng_ctx *ctx, spng_read_fn read_func, void *user)
{
    if(ctx == NULL || read_func == NULL) return 1;
    if(!ctx->valid_state) return SPNG_EBADSTATE;

    if(ctx->data != NULL) return SPNG_EBUF_SET;

    ctx->data = spng__malloc(ctx, 8192);
    if(ctx->data == NULL) return SPNG_EMEM;
    ctx->data_size = 8192;

    ctx->read_fn = read_func;
    ctx->read_user_ptr = user;

    ctx->streaming = 1;

    return 0;
}

int spng_set_image_limits(spng_ctx *ctx, uint32_t width, uint32_t height)
{
    if(ctx == NULL) return 1;

    if(width > png_u32max || height > png_u32max) return 1;

    ctx->max_width = width;
    ctx->max_height = height;

    return 0;
}

int spng_get_image_limits(spng_ctx *ctx, uint32_t *width, uint32_t *height)
{
    if(ctx == NULL || width == NULL || height == NULL) return 1;

    *width = ctx->max_width;
    *height = ctx->max_height;

    return 0;
}

int spng_set_chunk_limits(spng_ctx *ctx, size_t chunk_size, size_t cache_limit)
{
    if(ctx == NULL || chunk_size > png_u32max) return 1;

    ctx->max_chunk_size = chunk_size;

    ctx->chunk_cache_limit = cache_limit;

    return 0;
}

int spng_get_chunk_limits(spng_ctx *ctx, size_t *chunk_size, size_t *cache_limit)
{
    if(ctx == NULL || chunk_size == NULL) return 1;

    *chunk_size = ctx->max_chunk_size;

    *cache_limit = ctx->chunk_cache_limit;

    return 0;
}

int spng_set_crc_action(spng_ctx *ctx, int critical, int ancillary)
{
    if(ctx == NULL) return 1;

    if(critical > 2 || critical < 0) return 1;
    if(ancillary > 2 || ancillary < 0) return 1;

    if(critical == SPNG_CRC_DISCARD) return 1;

    ctx->crc_action_critical = critical;
    ctx->crc_action_ancillary = ancillary;

    return 0;
}

int spng_decoded_image_size(spng_ctx *ctx, int fmt, size_t *out)
{
    if(ctx == NULL || out == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    size_t res;
    if(fmt == SPNG_FMT_RGBA8)
    {
        if(4 > SIZE_MAX / ctx->ihdr.width) return SPNG_EOVERFLOW;
        res = 4 * ctx->ihdr.width;

        if(res > SIZE_MAX / ctx->ihdr.height) return SPNG_EOVERFLOW;
        res = res * ctx->ihdr.height;
    }
    else if(fmt == SPNG_FMT_RGBA16)
    {
        if(8 > SIZE_MAX / ctx->ihdr.width) return SPNG_EOVERFLOW;
        res = 8 * ctx->ihdr.width;

        if(res > SIZE_MAX / ctx->ihdr.height) return SPNG_EOVERFLOW;
        res = res * ctx->ihdr.height;
    }
    else return SPNG_EFMT;

    *out = res;

    return 0;
}

int calculate_subimages(struct spng_subimage sub[7], size_t *widest_scanline, struct spng_ihdr *ihdr, unsigned channels)
{
    if(sub == NULL || ihdr == NULL) return 1;

    if(ihdr->interlace_method == 1)
    {
        sub[0].width = (ihdr->width + 7) >> 3;
        sub[0].height = (ihdr->height + 7) >> 3;
        sub[1].width = (ihdr->width + 3) >> 3;
        sub[1].height = (ihdr->height + 7) >> 3;
        sub[2].width = (ihdr->width + 3) >> 2;
        sub[2].height = (ihdr->height + 3) >> 3;
        sub[3].width = (ihdr->width + 1) >> 2;
        sub[3].height = (ihdr->height + 3) >> 2;
        sub[4].width = (ihdr->width + 1) >> 1;
        sub[4].height = (ihdr->height + 1) >> 2;
        sub[5].width = ihdr->width >> 1;
        sub[5].height = (ihdr->height + 1) >> 1;
        sub[6].width = ihdr->width;
        sub[6].height = ihdr->height >> 1;
    }
    else
    {
        sub[0].width = ihdr->width;
        sub[0].height = ihdr->height;
    }

    size_t scanline_width, widest = 0;

    int i;
    for(i=0; i < 7; i++)
    {/* Calculate scanline width in bits, round up to the nearest byte */
        if(sub[i].width == 0 || sub[i].height == 0) continue;

        scanline_width = channels * ihdr->bit_depth;

        if(scanline_width > SIZE_MAX / ihdr->width) return SPNG_EOVERFLOW;
        scanline_width = scanline_width * sub[i].width;

        scanline_width += 8; /* Filter byte */

        if(scanline_width < 8) return SPNG_EOVERFLOW;

        /* Round up */
        if(scanline_width % 8 != 0)
        {
            scanline_width = scanline_width + 8;
            if(scanline_width < 8) return SPNG_EOVERFLOW;

            scanline_width -= (scanline_width % 8);
        }

        scanline_width /= 8;

        sub[i].scanline_width = scanline_width;

        if(widest < scanline_width) widest = scanline_width;
    }

    *widest_scanline = widest;

    return 0;
}

int check_ihdr(struct spng_ihdr *ihdr, uint32_t max_width, uint32_t max_height)
{
    if(ihdr->width > png_u32max || ihdr->width > max_width || !ihdr->width) return SPNG_EWIDTH;
    if(ihdr->height > png_u32max || ihdr->height > max_height || !ihdr->height) return SPNG_EHEIGHT;

    switch(ihdr->color_type)
    {
        case SPNG_COLOR_TYPE_GRAYSCALE:
        {
            if( !(ihdr->bit_depth == 1 || ihdr->bit_depth == 2 ||
                  ihdr->bit_depth == 4 || ihdr->bit_depth == 8 ||
                  ihdr->bit_depth == 16) )
                  return SPNG_EBIT_DEPTH;

            break;
        }
        case SPNG_COLOR_TYPE_TRUECOLOR:
        {
            if( !(ihdr->bit_depth == 8 || ihdr->bit_depth == 16) )
                return SPNG_EBIT_DEPTH;

            break;
        }
        case SPNG_COLOR_TYPE_INDEXED:
        {
            if( !(ihdr->bit_depth == 1 || ihdr->bit_depth == 2 ||
                  ihdr->bit_depth == 4 || ihdr->bit_depth == 8) )
                return SPNG_EBIT_DEPTH;

            break;
        }
        case SPNG_COLOR_TYPE_GRAYSCALE_ALPHA:
        {
            if( !(ihdr->bit_depth == 8 || ihdr->bit_depth == 16) )
                return SPNG_EBIT_DEPTH;

            break;
        }
        case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA:
        {
            if( !(ihdr->bit_depth == 8 || ihdr->bit_depth == 16) )
                return SPNG_EBIT_DEPTH;

            break;
        }
    default: return SPNG_ECOLOR_TYPE;
    }

    if(ihdr->compression_method || ihdr->filter_method)
        return SPNG_ECOMPRESSION_METHOD;

    if( !(ihdr->interlace_method == 0 || ihdr->interlace_method == 1) )
        return SPNG_EINTERLACE_METHOD;

    return 0;
}

int check_sbit(struct spng_sbit *sbit, struct spng_ihdr *ihdr)
{
    if(sbit == NULL || ihdr == NULL) return 1;

    if(ihdr->color_type == 0)
    {
        if(sbit->grayscale_bits == 0) return SPNG_ESBIT;
        if(sbit->grayscale_bits > ihdr->bit_depth) return SPNG_ESBIT;
    }
    else if(ihdr->color_type == 2 || ihdr->color_type == 3)
    {
        if(sbit->red_bits == 0) return SPNG_ESBIT;
        if(sbit->green_bits == 0) return SPNG_ESBIT;
        if(sbit->blue_bits == 0) return SPNG_ESBIT;

        uint8_t bit_depth;
        if(ihdr->color_type == 3) bit_depth = 8;
        else bit_depth = ihdr->bit_depth;

        if(sbit->red_bits > bit_depth) return SPNG_ESBIT;
        if(sbit->green_bits > bit_depth) return SPNG_ESBIT;
        if(sbit->blue_bits > bit_depth) return SPNG_ESBIT;
    }
    else if(ihdr->color_type == 4)
    {
        if(sbit->grayscale_bits == 0) return SPNG_ESBIT;
        if(sbit->grayscale_bits > ihdr->bit_depth) return SPNG_ESBIT;
    }
    else if(ihdr->color_type == 6)
    {
        if(sbit->red_bits == 0) return SPNG_ESBIT;
        if(sbit->green_bits == 0) return SPNG_ESBIT;
        if(sbit->blue_bits == 0) return SPNG_ESBIT;
        if(sbit->alpha_bits == 0) return SPNG_ESBIT;

        if(sbit->red_bits > ihdr->bit_depth) return SPNG_ESBIT;
        if(sbit->green_bits > ihdr->bit_depth) return SPNG_ESBIT;
        if(sbit->blue_bits > ihdr->bit_depth) return SPNG_ESBIT;
        if(sbit->alpha_bits > ihdr->bit_depth) return SPNG_ESBIT;
    }

    return 0;
}

int check_chrm_int(struct spng_chrm_int *chrm_int)
{
    if(chrm_int == NULL) return 1;

    if(chrm_int->white_point_x > png_u32max ||
       chrm_int->white_point_y > png_u32max ||
       chrm_int->red_x > png_u32max ||
       chrm_int->red_y > png_u32max ||
       chrm_int->green_x  > png_u32max ||
       chrm_int->green_y  > png_u32max ||
       chrm_int->blue_x > png_u32max ||
       chrm_int->blue_y > png_u32max) return SPNG_ECHRM;

    return 0;
}

int check_phys(struct spng_phys *phys)
{
    if(phys == NULL) return 1;

    if(phys->unit_specifier > 1) return SPNG_EPHYS;

    if(phys->ppu_x > png_u32max) return SPNG_EPHYS;
    if(phys->ppu_y > png_u32max) return SPNG_EPHYS;

    return 0;
}

int check_time(struct spng_time *time)
{
    if(time == NULL) return 1;

    if(time->month == 0 || time->month > 12) return 1;
    if(time->day == 0 || time->day > 31) return 1;
    if(time->hour > 23) return 1;
    if(time->minute > 59) return 1;
    if(time->second > 60) return 1;

    return 0;
}

int check_offs(struct spng_offs *offs)
{
    if(offs == NULL) return 1;

    if(offs->x < png_s32min || offs->y < png_s32min) return 1;
    if(offs->unit_specifier > 1) return 1;

    return 0;
}

int check_exif(struct spng_exif *exif)
{
    if(exif == NULL) return 1;

    if(exif->length < 4) return SPNG_ECHUNK_SIZE;
    if(exif->length > png_u32max) return SPNG_ECHUNK_SIZE;

    const uint8_t exif_le[4] = { 73, 73, 42, 0 };
    const uint8_t exif_be[4] = { 77, 77, 0, 42 };

    if(memcmp(exif->data, exif_le, 4) && memcmp(exif->data, exif_be, 4)) return 1;

    return 0;
}

/* Validate PNG keyword *str, *str must be 80 bytes */
int check_png_keyword(const char str[80])
{
    if(str == NULL) return 1;
    char *end = memchr(str, '\0', 80);

    if(end == NULL) return 1; /* unterminated string */
    if(end == str) return 1; /* zero-length string */
    if(str[0] == ' ') return 1; /* leading space */
    if(end[-1] == ' ') return 1; /* trailing space */
    if(strstr(str, "  ") != NULL) return 1; /* consecutive spaces */

    uint8_t c;
    while(str != end)
    {
        memcpy(&c, str, 1);

        if( (c >= 32 && c <= 126) || (c >= 161) ) str++;
        else return 1; /* invalid character */
    }

    return 0;
}

/* Validate PNG text *str up to 'len' bytes */
int check_png_text(const char *str, size_t len)
{/* XXX: are consecutive newlines permitted? */
    if(str == NULL || len == 0) return 1;

    uint8_t c;
    size_t i = 0;
    while(i < len)
    {
        memcpy(&c, str + i, 1);

        if( (c >= 32 && c <= 126) || (c >= 161) || c == 10) i++;
        else return 1; /* invalid character */
    }

    return 0;
}

int spng_get_ihdr(spng_ctx *ctx, struct spng_ihdr *ihdr)
{
    SPNG_GET_CHUNK_BOILERPLATE(ihdr);

    memcpy(ihdr, &ctx->ihdr, sizeof(struct spng_ihdr));

    return 0;
}

int spng_get_plte(spng_ctx *ctx, struct spng_plte *plte)
{
    SPNG_GET_CHUNK_BOILERPLATE(plte);

    if(!ctx->stored_plte) return SPNG_ECHUNKAVAIL;

    memcpy(plte, &ctx->plte, sizeof(struct spng_plte));

    return 0;
}

int spng_get_trns(spng_ctx *ctx, struct spng_trns *trns)
{
    SPNG_GET_CHUNK_BOILERPLATE(trns);

    if(!ctx->stored_trns) return SPNG_ECHUNKAVAIL;

    memcpy(trns, &ctx->trns, sizeof(struct spng_trns));

    return 0;
}

int spng_get_chrm(spng_ctx *ctx, struct spng_chrm *chrm)
{
    SPNG_GET_CHUNK_BOILERPLATE(chrm);

    if(!ctx->stored_chrm) return SPNG_ECHUNKAVAIL;

    chrm->white_point_x = (double)ctx->chrm_int.white_point_x / 100000.0;
    chrm->white_point_y = (double)ctx->chrm_int.white_point_y / 100000.0;
    chrm->red_x = (double)ctx->chrm_int.red_x / 100000.0;
    chrm->red_y = (double)ctx->chrm_int.red_y / 100000.0;
    chrm->blue_y = (double)ctx->chrm_int.blue_y / 100000.0;
    chrm->blue_x = (double)ctx->chrm_int.blue_x / 100000.0;
    chrm->green_x = (double)ctx->chrm_int.green_x / 100000.0;
    chrm->green_y = (double)ctx->chrm_int.green_y / 100000.0;

    return 0;
}

int spng_get_chrm_int(spng_ctx *ctx, struct spng_chrm_int *chrm)
{
    SPNG_GET_CHUNK_BOILERPLATE(chrm);

    if(!ctx->stored_chrm) return SPNG_ECHUNKAVAIL;

    memcpy(chrm, &ctx->chrm_int, sizeof(struct spng_chrm_int));

    return 0;
}

int spng_get_gama(spng_ctx *ctx, double *gamma)
{
    SPNG_GET_CHUNK_BOILERPLATE(gamma);

    if(!ctx->stored_gama) return SPNG_ECHUNKAVAIL;

    *gamma = (double)ctx->gama / 100000.0;

    return 0;
}

int spng_get_iccp(spng_ctx *ctx, struct spng_iccp *iccp)
{
    SPNG_GET_CHUNK_BOILERPLATE(iccp);

    if(!ctx->stored_iccp) return SPNG_ECHUNKAVAIL;

    memcpy(iccp, &ctx->iccp, sizeof(struct spng_iccp));

    return 0;
}

int spng_get_sbit(spng_ctx *ctx, struct spng_sbit *sbit)
{
    SPNG_GET_CHUNK_BOILERPLATE(sbit);

    if(!ctx->stored_sbit) return SPNG_ECHUNKAVAIL;

    memcpy(sbit, &ctx->sbit, sizeof(struct spng_sbit));

    return 0;
}

int spng_get_srgb(spng_ctx *ctx, uint8_t *rendering_intent)
{
    SPNG_GET_CHUNK_BOILERPLATE(rendering_intent);

    if(!ctx->stored_srgb) return SPNG_ECHUNKAVAIL;

    *rendering_intent = ctx->srgb_rendering_intent;

    return 0;
}

int spng_get_text(spng_ctx *ctx, struct spng_text *text, uint32_t *n_text)
{
    if(ctx == NULL || n_text == NULL) return 1;

    if(text == NULL)
    {
        *n_text = ctx->n_text;
        return 0;
    }

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->stored_text) return SPNG_ECHUNKAVAIL;
    if(*n_text < ctx->n_text) return 1;

    memcpy(text, &ctx->text_list, ctx->n_text * sizeof(struct spng_text));

    return ret;
}

int spng_get_bkgd(spng_ctx *ctx, struct spng_bkgd *bkgd)
{
    SPNG_GET_CHUNK_BOILERPLATE(bkgd);

    if(!ctx->stored_bkgd) return SPNG_ECHUNKAVAIL;

    memcpy(bkgd, &ctx->bkgd, sizeof(struct spng_bkgd));

    return 0;
}

int spng_get_hist(spng_ctx *ctx, struct spng_hist *hist)
{
    SPNG_GET_CHUNK_BOILERPLATE(hist);

    if(!ctx->stored_hist) return SPNG_ECHUNKAVAIL;

    memcpy(hist, &ctx->hist, sizeof(struct spng_hist));

    return 0;
}

int spng_get_phys(spng_ctx *ctx, struct spng_phys *phys)
{
    SPNG_GET_CHUNK_BOILERPLATE(phys);

    if(!ctx->stored_phys) return SPNG_ECHUNKAVAIL;

    memcpy(phys, &ctx->phys, sizeof(struct spng_phys));

    return 0;
}

int spng_get_splt(spng_ctx *ctx, struct spng_splt *splt, uint32_t *n_splt)
{
    if(ctx == NULL || n_splt == NULL) return 1;

    if(splt == NULL)
    {
        *n_splt = ctx->n_splt;
        return 0;
    }

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->stored_splt) return SPNG_ECHUNKAVAIL;
    if(*n_splt < ctx->n_splt) return 1;

    memcpy(splt, &ctx->splt_list, ctx->n_splt * sizeof(struct spng_splt));

    return 0;
}

int spng_get_time(spng_ctx *ctx, struct spng_time *time)
{
    SPNG_GET_CHUNK_BOILERPLATE(time);

    if(!ctx->stored_time) return SPNG_ECHUNKAVAIL;

    memcpy(time, &ctx->time, sizeof(struct spng_time));

    return 0;
}

int spng_get_offs(spng_ctx *ctx, struct spng_offs *offs)
{
    SPNG_GET_CHUNK_BOILERPLATE(offs);

    if(!ctx->stored_offs) return SPNG_ECHUNKAVAIL;

    memcpy(offs, &ctx->offs, sizeof(struct spng_offs));

    return 0;
}

int spng_get_exif(spng_ctx *ctx, struct spng_exif *exif)
{
    SPNG_GET_CHUNK_BOILERPLATE(exif);

    if(!ctx->stored_exif) return SPNG_ECHUNKAVAIL;

    memcpy(exif, &ctx->exif, sizeof(struct spng_exif));

    return 0;
}

int spng_set_ihdr(spng_ctx *ctx, struct spng_ihdr *ihdr)
{
    SPNG_SET_CHUNK_BOILERPLATE(ihdr);

    if(ctx->stored_ihdr) return 1;

    ret = check_ihdr(ihdr, ctx->max_width, ctx->max_height);
    if(ret) return ret;

    memcpy(&ctx->ihdr, ihdr, sizeof(struct spng_ihdr));

    ctx->stored_ihdr = 1;
    ctx->user_ihdr = 1;

    ctx->encode_only = 1;

    return 0;
}

int spng_set_plte(spng_ctx *ctx, struct spng_plte *plte)
{
    SPNG_SET_CHUNK_BOILERPLATE(plte);

    if(!ctx->stored_ihdr) return 1;

    if(plte->n_entries == 0) return 1;
    if(plte->n_entries > 256) return 1;

    if(ctx->ihdr.color_type == 3)
    {
        if(plte->n_entries > (1 << ctx->ihdr.bit_depth)) return 1;
    }

    memcpy(&ctx->plte, plte, sizeof(struct spng_plte));

    ctx->stored_plte = 1;
    ctx->user_plte = 1;

    return 0;
}

int spng_set_trns(spng_ctx *ctx, struct spng_trns *trns)
{
    SPNG_SET_CHUNK_BOILERPLATE(trns);

    if(!ctx->stored_ihdr) return 1;

    uint16_t mask = ~0;
    if(ctx->ihdr.bit_depth < 16) mask = (1 << ctx->ihdr.bit_depth) - 1;

    if(ctx->ihdr.color_type == 0)
    {
        trns->gray &= mask;
    }
    else if(ctx->ihdr.color_type == 2)
    {
        trns->red &= mask;
        trns->green &= mask;
        trns->blue &= mask;
    }
    else if(ctx->ihdr.color_type == 3)
    {
        if(!ctx->stored_plte) return SPNG_ETRNS_NO_PLTE;
    }
    else return SPNG_ETRNS_COLOR_TYPE;

    memcpy(&ctx->trns, trns, sizeof(struct spng_trns));

    ctx->stored_trns = 1;
    ctx->user_trns = 1;

    return 0;
}

int spng_set_chrm(spng_ctx *ctx, struct spng_chrm *chrm)
{
    SPNG_SET_CHUNK_BOILERPLATE(chrm);

    struct spng_chrm_int chrm_int;

    chrm_int.white_point_x = chrm->white_point_x * 100000.0;
    chrm_int.white_point_y = chrm->white_point_y * 100000.0;
    chrm_int.red_x = chrm->red_x * 100000.0;
    chrm_int.red_y = chrm->red_y * 100000.0;
    chrm_int.green_x = chrm->green_x * 100000.0;
    chrm_int.green_y = chrm->green_y * 100000.0;
    chrm_int.blue_x = chrm->blue_x * 100000.0;
    chrm_int.blue_y = chrm->blue_y * 100000.0;

    if(check_chrm_int(&chrm_int)) return SPNG_ECHRM;

    memcpy(&ctx->chrm_int, &chrm_int, sizeof(struct spng_chrm_int));

    ctx->stored_chrm = 1;
    ctx->user_chrm = 1;

    return 0;
}

int spng_set_chrm_int(spng_ctx *ctx, struct spng_chrm_int *chrm_int)
{
    SPNG_SET_CHUNK_BOILERPLATE(chrm_int);

    if(check_chrm_int(chrm_int)) return SPNG_ECHRM;

    memcpy(&ctx->chrm_int, chrm_int, sizeof(struct spng_chrm_int));

    ctx->stored_chrm = 1;
    ctx->user_chrm = 1;

    return 0;
}

int spng_set_gama(spng_ctx *ctx, double gamma)
{
    SPNG_SET_CHUNK_BOILERPLATE(ctx);

    uint32_t gama = gamma * 100000.0;

    if(!gama) return 1;
    if(gama > png_u32max) return 1;

    ctx->gama = gama;

    ctx->stored_gama = 1;
    ctx->user_gama = 1;

    return 0;
}

int spng_set_iccp(spng_ctx *ctx, struct spng_iccp *iccp)
{
    SPNG_SET_CHUNK_BOILERPLATE(iccp);

    if(check_png_keyword(iccp->profile_name)) return SPNG_EICCP_NAME;
    if(!iccp->profile_len) return 1;

    if(ctx->iccp.profile && !ctx->user_iccp) spng__free(ctx, ctx->iccp.profile);

    memcpy(&ctx->iccp, iccp, sizeof(struct spng_iccp));

    ctx->stored_iccp = 1;
    ctx->user_iccp = 1;

    return 0;
}

int spng_set_sbit(spng_ctx *ctx, struct spng_sbit *sbit)
{
    SPNG_SET_CHUNK_BOILERPLATE(sbit);

    if(!ctx->stored_ihdr) return 1;
    if(check_sbit(sbit, &ctx->ihdr)) return 1;

    memcpy(&ctx->sbit, sbit, sizeof(struct spng_sbit));

    ctx->stored_sbit = 1;
    ctx->user_sbit = 1;

    return 0;
}

int spng_set_srgb(spng_ctx *ctx, uint8_t rendering_intent)
{
    SPNG_SET_CHUNK_BOILERPLATE(ctx);

    if(rendering_intent > 3) return 1;

    ctx->srgb_rendering_intent = rendering_intent;

    ctx->stored_srgb = 1;
    ctx->user_srgb = 1;

    return 0;
}

int spng_set_text(spng_ctx *ctx, struct spng_text *text, uint32_t n_text)
{
    if(ctx == NULL || text == NULL || !n_text) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    uint32_t i;
    for(i=0; i < n_text; i++)
    {
        if(check_png_keyword(text[i].keyword)) return SPNG_ETEXT_KEYWORD;
        if(!text[i].length) return 1;
        if(text[i].text == NULL) return 1;

        if(text[i].type == SPNG_TEXT)
        {
            if(check_png_text(text[i].text, text[i].length)) return 1;
        }
        else if(text[i].type == SPNG_ZTXT)
        {
            if(check_png_text(text[i].text, text[i].length)) return 1;

            if(text[i].compression_method != 0) return 1;
        }
        else if(text[i].type == SPNG_ITXT)
        {
            if(text[i].compression_flag > 1) return 1;
            if(text[i].compression_method != 0) return 1;
            if(text[i].language_tag == NULL) return SPNG_EITXT_LANG_TAG;
            if(text[i].translated_keyword == NULL) return SPNG_EITXT_TRANSLATED_KEY;

        }
        else return 1;

    }


    if(ctx->text_list != NULL && !ctx->user_text)
    {
        for(i=0; i< ctx->n_text; i++)
        {
            if(ctx->text_list[i].text != NULL) spng__free(ctx, ctx->text_list[i].text);
            if(ctx->text_list[i].language_tag != NULL) spng__free(ctx, ctx->text_list[i].language_tag);
            if(ctx->text_list[i].translated_keyword != NULL) spng__free(ctx, ctx->text_list[i].translated_keyword);
        }
        spng__free(ctx, ctx->text_list);
    }

    ctx->text_list = text;
    ctx->n_text = n_text;

    ctx->stored_text = 1;
    ctx->user_text = 1;

    return 0;
}

int spng_set_bkgd(spng_ctx *ctx, struct spng_bkgd *bkgd)
{
    SPNG_SET_CHUNK_BOILERPLATE(bkgd);

    if(!ctx->stored_ihdr) return 1;

    uint16_t mask = ~0;

    if(ctx->ihdr.bit_depth < 16) mask = (1 << ctx->ihdr.bit_depth) - 1;

    if(ctx->ihdr.color_type == 0 || ctx->ihdr.color_type == 4)
    {
        bkgd->gray &= mask;
    }
    else if(ctx->ihdr.color_type == 2 || ctx->ihdr.color_type == 6)
    {
        bkgd->red &= mask;
        bkgd->green &= mask;
        bkgd->blue &= mask;
    }
    else if(ctx->ihdr.color_type == 3)
    {
        if(!ctx->stored_plte) return SPNG_EBKGD_NO_PLTE;
        if(bkgd->plte_index >= ctx->plte.n_entries) return SPNG_EBKGD_PLTE_IDX;
    }

    memcpy(&ctx->bkgd, bkgd, sizeof(struct spng_bkgd));

    ctx->stored_bkgd = 1;
    ctx->user_bkgd = 1;

    return 0;
}

int spng_set_hist(spng_ctx *ctx, struct spng_hist *hist)
{
    SPNG_SET_CHUNK_BOILERPLATE(hist);

    if(!ctx->stored_plte) return SPNG_EHIST_NO_PLTE;

    memcpy(&ctx->hist, hist, sizeof(struct spng_hist));

    ctx->stored_hist = 1;
    ctx->user_hist = 1;

    return 0;
}

int spng_set_phys(spng_ctx *ctx, struct spng_phys *phys)
{
    SPNG_SET_CHUNK_BOILERPLATE(phys);

    if(check_phys(phys)) return SPNG_EPHYS;

    memcpy(&ctx->phys, phys, sizeof(struct spng_phys));

    ctx->stored_phys = 1;
    ctx->user_phys = 1;

    return 0;
}

int spng_set_splt(spng_ctx *ctx, struct spng_splt *splt, uint32_t n_splt)
{
    if(ctx == NULL || splt == NULL || !n_splt) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    uint32_t i;
    for(i=0; i < n_splt; i++)
    {
        if(check_png_keyword(splt[i].name)) return SPNG_ESPLT_NAME;
        if( !(splt[i].sample_depth == 8 || splt[i].sample_depth == 16) ) return SPNG_ESPLT_DEPTH;
    }

    if(ctx->stored_splt && !ctx->user_splt)
    {
        for(i=0; i < ctx->n_splt; i++)
        {
            if(ctx->splt_list[i].entries != NULL) spng__free(ctx, ctx->splt_list[i].entries);
        }
        spng__free(ctx, ctx->splt_list);
    }

    ctx->splt_list = splt;
    ctx->n_splt = n_splt;

    ctx->stored_splt = 1;
    ctx->user_splt = 1;

    return 0;
}

int spng_set_time(spng_ctx *ctx, struct spng_time *time)
{
    SPNG_SET_CHUNK_BOILERPLATE(time);

    if(check_time(time)) return SPNG_ETIME;

    memcpy(&ctx->time, time, sizeof(struct spng_time));

    ctx->stored_time = 1;
    ctx->user_time = 1;

    return 0;
}

int spng_set_offs(spng_ctx *ctx, struct spng_offs *offs)
{
    SPNG_SET_CHUNK_BOILERPLATE(offs);

    if(check_offs(offs)) return SPNG_EOFFS;

    memcpy(&ctx->offs, offs, sizeof(struct spng_offs));

    ctx->stored_offs = 1;
    ctx->user_offs = 1;

    return 0;
}

int spng_set_exif(spng_ctx *ctx, struct spng_exif *exif)
{
    SPNG_SET_CHUNK_BOILERPLATE(exif);

    if(check_exif(exif)) return SPNG_EEXIF;

    if(ctx->exif.data != NULL && !ctx->user_exif) spng__free(ctx, ctx->exif.data);

    memcpy(&ctx->exif, exif, sizeof(struct spng_exif));

    ctx->stored_exif = 1;
    ctx->user_exif = 1;

    return 0;
}

const char *spng_strerror(int err)
{
    switch(err)
    {
        case SPNG_IO_EOF: return "end of stream";
        case SPNG_IO_ERROR: return "stream error";
        case SPNG_OK: return "success";
        case SPNG_EINVAL: return "invalid argument";
        case SPNG_EMEM: return "out of memory";
        case SPNG_EOVERFLOW: return "arithmetic overflow";
        case SPNG_ESIGNATURE: return "invalid signature";
        case SPNG_EWIDTH: return "invalid image width";
        case SPNG_EHEIGHT: return "invalid image height";
        case SPNG_EUSER_WIDTH: return "image width exceeds user limit";
        case SPNG_EUSER_HEIGHT: return "image height exceeds user limit";
        case SPNG_EBIT_DEPTH: return "invalid bit depth";
        case SPNG_ECOLOR_TYPE: return "invalid color type";
        case SPNG_ECOMPRESSION_METHOD: return "invalid compression method";
        case SPNG_EFILTER_METHOD: return "invalid filter method";
        case SPNG_EINTERLACE_METHOD: return "invalid interlace method";
        case SPNG_EIHDR_SIZE: return "invalid IHDR chunk size";
        case SPNG_ENOIHDR: return "missing IHDR chunk";
        case SPNG_ECHUNK_POS: return "invalid chunk position";
        case SPNG_ECHUNK_SIZE: return "invalid chunk length";
        case SPNG_ECHUNK_CRC: return "invalid chunk checksum";
        case SPNG_ECHUNK_TYPE: return "invalid chunk type";
        case SPNG_ECHUNK_UNKNOWN_CRITICAL: return "unknown critical chunk";
        case SPNG_EDUP_PLTE: return "duplicate PLTE chunk";
        case SPNG_EDUP_CHRM: return "duplicate cHRM chunk";
        case SPNG_EDUP_GAMA: return "duplicate gAMA chunk";
        case SPNG_EDUP_ICCP: return "duplicate iCCP chunk";
        case SPNG_EDUP_SBIT: return "duplicate sBIT chunk";
        case SPNG_EDUP_SRGB: return "duplicate sRGB chunk";
        case SPNG_EDUP_BKGD: return "duplicate bKGD chunk";
        case SPNG_EDUP_HIST: return "duplicate hIST chunk";
        case SPNG_EDUP_TRNS: return "duplicate tRNS chunk";
        case SPNG_EDUP_PHYS: return "duplicate pHYs chunk";
        case SPNG_EDUP_TIME: return "duplicate tIME chunk";
        case SPNG_EDUP_OFFS: return "duplicate oFFs chunk";
        case SPNG_EDUP_EXIF: return "duplicate eXIf chunk";
        case SPNG_ECHRM: return "invalid cHRM chunk";
        case SPNG_EPLTE_IDX: return "invalid palette (PLTE) index";
        case SPNG_ETRNS_COLOR_TYPE: return "tRNS chunk with incompatible color type";
        case SPNG_ETRNS_NO_PLTE: return "missing palette (PLTE) for tRNS chunk";
        case SPNG_EGAMA: return "invalid gAMA chunk";
        case SPNG_EICCP_NAME: return "invalid iCCP profile name";
        case SPNG_EICCP_COMPRESSION_METHOD: return "invalid iCCP compression method";
        case SPNG_ESBIT: return "invalid sBIT chunk";
        case SPNG_ESRGB: return "invalid sRGB chunk";
        case SPNG_ETEXT: return "invalid tEXt chunk";
        case SPNG_ETEXT_KEYWORD: return "invalid tEXt keyword";
        case SPNG_EZTXT: return "invalid zTXt chunk";
        case SPNG_EZTXT_COMPRESSION_METHOD: return "invalid zTXt compression method";
        case SPNG_EITXT: return "invalid iTXt chunk";
        case SPNG_EITXT_COMPRESSION_FLAG: return "invalid iTXt compression flag";
        case SPNG_EITXT_COMPRESSION_METHOD: return "invalid iTXt compression method";
        case SPNG_EITXT_LANG_TAG: return "invalid iTXt language tag";
        case SPNG_EITXT_TRANSLATED_KEY: return "invalid iTXt translated key";
        case SPNG_EBKGD_NO_PLTE: return "missing palette for bKGD chunk";
        case SPNG_EBKGD_PLTE_IDX: return "invalid palette index for bKGD chunk";
        case SPNG_EHIST_NO_PLTE: return "missing palette for hIST chunk";
        case SPNG_EPHYS: return "invalid pHYs chunk";
        case SPNG_ESPLT_NAME: return "invalid suggested palette name";
        case SPNG_ESPLT_DUP_NAME: return "duplicate suggested palette (sPLT) name";
        case SPNG_ESPLT_DEPTH: return "invalid suggested palette (sPLT) sample depth";
        case SPNG_ETIME: return "invalid tIME chunk";
        case SPNG_EOFFS: return "invalid oFFs chunk";
        case SPNG_EEXIF: return "invalid eXIf chunk";
        case SPNG_EIDAT_TOO_SHORT: return "IDAT stream too short";
        case SPNG_EIDAT_STREAM: return "IDAT stream error";
        case SPNG_EZLIB: return "zlib error";
        case SPNG_EFILTER: return "invalid scanline filter";
        case SPNG_EBUFSIZ: return "output buffer too small";
        case SPNG_EIO: return "i/o error";
        case SPNG_EOF: return "end of file";
        case SPNG_EBUF_SET: return "buffer already set";
        case SPNG_EBADSTATE: return "non-recoverable state";
        case SPNG_EFMT: return "invalid format";
        case SPNG_EFLAGS: return "invalid flags";
        case SPNG_ECHUNKAVAIL: return "chunk not available";
        case SPNG_ENCODE_ONLY: return "encode only context";
        default: return "unknown error";
    }
}

const char *spng_version_string(void)
{
    return SPNG_VERSION_STRING;
}

/* filter_sse2_intrinsics.c - SSE2 optimized filter functions
 *
 * Copyright (c) 2018 Cosmin Truta
 * Copyright (c) 2016-2017 Glenn Randers-Pehrson
 * Written by Mike Klein and Matt Sarett
 * Derived from arm/filter_neon_intrinsics.c
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */

#if defined(SPNG_OPTIMIZE_FILTER) && defined(SPNG_X86)

#if defined(__GNUC__) && !defined(__clang__)
   #pragma GCC target("sse2")
   #pragma GCC target("ssse3")
#endif

#define _mm_blendv_epi8__SSSE3__ 1

#include <immintrin.h>
#include <inttypes.h>
#include <string.h>

#define PNG_INTEL_SSE_IMPLEMENTATION 3

typedef unsigned char png_byte;
typedef uint16_t png_uint_16;
typedef uint32_t png_uint_32;
typedef unsigned char* png_bytep;
typedef const unsigned char* png_const_bytep;

/* Functions in this file look at most 3 pixels (a,b,c) to predict the 4th (d).
 * They're positioned like this:
 *    prev:  c b
 *    row:   a d
 * The Sub filter predicts d=a, Avg d=(a+b)/2, and Paeth predicts d to be
 * whichever of a, b, or c is closest to p=a+b-c.
 */

static __m128i load4(const void* p)
{
   int tmp;
   memcpy(&tmp, p, sizeof(tmp));
   return _mm_cvtsi32_si128(tmp);
}

static void store4(void* p, __m128i v)
{
   int tmp = _mm_cvtsi128_si32(v);
   memcpy(p, &tmp, sizeof(int));
}

static __m128i load3(const void* p)
{
   png_uint_32 tmp = 0;
   memcpy(&tmp, p, 3);
   return _mm_cvtsi32_si128(tmp);
}

static void store3(void* p, __m128i v)
{
   int tmp = _mm_cvtsi128_si32(v);
   memcpy(p, &tmp, 3);
}

void png_read_filter_row_sub3(size_t rowbytes, png_bytep row)
{
   /* The Sub filter predicts each pixel as the previous pixel, a.
    * There is no pixel to the left of the first pixel.  It's encoded directly.
    * That works with our main loop if we just say that left pixel was zero.
    */
   size_t rb = rowbytes;

   __m128i a, d = _mm_setzero_si128();

   while (rb >= 4) {
      a = d; d = load4(row);
      d = _mm_add_epi8(d, a);
      store3(row, d);

      row += 3;
      rb  -= 3;
   }
   if (rb > 0) {
      a = d; d = load3(row);
      d = _mm_add_epi8(d, a);
      store3(row, d);
   }
}

void png_read_filter_row_sub4(size_t rowbytes, png_bytep row)
{
   /* The Sub filter predicts each pixel as the previous pixel, a.
    * There is no pixel to the left of the first pixel.  It's encoded directly.
    * That works with our main loop if we just say that left pixel was zero.
    */
   size_t rb = rowbytes+4;

   __m128i a, d = _mm_setzero_si128();

   while (rb > 4) {
      a = d; d = load4(row);
      d = _mm_add_epi8(d, a);
      store4(row, d);

      row += 4;
      rb  -= 4;
   }
}

void png_read_filter_row_avg3(size_t rowbytes, png_bytep row, png_const_bytep prev)
{
   /* The Avg filter predicts each pixel as the (truncated) average of a and b.
    * There's no pixel to the left of the first pixel.  Luckily, it's
    * predicted to be half of the pixel above it.  So again, this works
    * perfectly with our loop if we make sure a starts at zero.
    */

   size_t rb = rowbytes;

   const __m128i zero = _mm_setzero_si128();

   __m128i    b;
   __m128i a, d = zero;

   while (rb >= 4)
   {
      __m128i avg;
             b = load4(prev);
      a = d; d = load4(row );

      /* PNG requires a truncating average, so we can't just use _mm_avg_epu8 */
      avg = _mm_avg_epu8(a,b);
      /* ...but we can fix it up by subtracting off 1 if it rounded up. */
      avg = _mm_sub_epi8(avg, _mm_and_si128(_mm_xor_si128(a, b),
                                            _mm_set1_epi8(1)));
      d = _mm_add_epi8(d, avg);
      store3(row, d);

      prev += 3;
      row  += 3;
      rb   -= 3;
   }

   if (rb > 0)
   {
      __m128i avg;
             b = load3(prev);
      a = d; d = load3(row );

      /* PNG requires a truncating average, so we can't just use _mm_avg_epu8 */
      avg = _mm_avg_epu8(a, b);
      /* ...but we can fix it up by subtracting off 1 if it rounded up. */
      avg = _mm_sub_epi8(avg, _mm_and_si128(_mm_xor_si128(a, b),
                                            _mm_set1_epi8(1)));

      d = _mm_add_epi8(d, avg);
      store3(row, d);
   }
}

void png_read_filter_row_avg4(size_t rowbytes, png_bytep row, png_const_bytep prev)
{
   /* The Avg filter predicts each pixel as the (truncated) average of a and b.
    * There's no pixel to the left of the first pixel.  Luckily, it's
    * predicted to be half of the pixel above it.  So again, this works
    * perfectly with our loop if we make sure a starts at zero.
    */
   size_t rb = rowbytes+4;

   const __m128i zero = _mm_setzero_si128();
   __m128i    b;
   __m128i a, d = zero;

   while (rb > 4)
   {
      __m128i avg;
             b = load4(prev);
      a = d; d = load4(row );

      /* PNG requires a truncating average, so we can't just use _mm_avg_epu8 */
      avg = _mm_avg_epu8(a,b);
      /* ...but we can fix it up by subtracting off 1 if it rounded up. */
      avg = _mm_sub_epi8(avg, _mm_and_si128(_mm_xor_si128(a, b),
                                            _mm_set1_epi8(1)));

      d = _mm_add_epi8(d, avg);
      store4(row, d);

      prev += 4;
      row  += 4;
      rb   -= 4;
   }
}

/* Returns |x| for 16-bit lanes. */
static __attribute__((target("ssse3"))) __m128i abs_i16(__m128i x)
{
#if PNG_INTEL_SSE_IMPLEMENTATION >= 2
   return _mm_abs_epi16(x);
#else
   /* Read this all as, return x<0 ? -x : x.
   * To negate two's complement, you flip all the bits then add 1.
    */
   __m128i is_negative = _mm_cmplt_epi16(x, _mm_setzero_si128());

   /* Flip negative lanes. */
   x = _mm_xor_si128(x, is_negative);

   /* +1 to negative lanes, else +0. */
   x = _mm_sub_epi16(x, is_negative);
   return x;
#endif
}

/* Bytewise c ? t : e. */
static __m128i if_then_else(__m128i c, __m128i t, __m128i e)
{
#if PNG_INTEL_SSE_IMPLEMENTATION > 3
   return _mm_blendv_epi8(e, t, c);
#else
   return _mm_or_si128(_mm_and_si128(c, t), _mm_andnot_si128(c, e));
#endif
}

void png_read_filter_row_paeth3(size_t rowbytes, png_bytep row, png_const_bytep prev)
{
   /* Paeth tries to predict pixel d using the pixel to the left of it, a,
    * and two pixels from the previous row, b and c:
    *   prev: c b
    *   row:  a d
    * The Paeth function predicts d to be whichever of a, b, or c is nearest to
    * p=a+b-c.
    *
    * The first pixel has no left context, and so uses an Up filter, p = b.
    * This works naturally with our main loop's p = a+b-c if we force a and c
    * to zero.
    * Here we zero b and d, which become c and a respectively at the start of
    * the loop.
    */
   size_t rb = rowbytes;
   const __m128i zero = _mm_setzero_si128();
   __m128i c, b = zero,
           a, d = zero;

   while (rb >= 4)
   {
      /* It's easiest to do this math (particularly, deal with pc) with 16-bit
       * intermediates.
       */
      __m128i pa,pb,pc,smallest,nearest;
      c = b; b = _mm_unpacklo_epi8(load4(prev), zero);
      a = d; d = _mm_unpacklo_epi8(load4(row ), zero);

      /* (p-a) == (a+b-c - a) == (b-c) */

      pa = _mm_sub_epi16(b, c);

      /* (p-b) == (a+b-c - b) == (a-c) */
      pb = _mm_sub_epi16(a, c);

      /* (p-c) == (a+b-c - c) == (a+b-c-c) == (b-c)+(a-c) */
      pc = _mm_add_epi16(pa, pb);

      pa = abs_i16(pa);  /* |p-a| */
      pb = abs_i16(pb);  /* |p-b| */
      pc = abs_i16(pc);  /* |p-c| */

      smallest = _mm_min_epi16(pc, _mm_min_epi16(pa, pb));

      /* Paeth breaks ties favoring a over b over c. */
      nearest  = if_then_else(_mm_cmpeq_epi16(smallest, pa), a,
                         if_then_else(_mm_cmpeq_epi16(smallest, pb), b, c));

      /* Note `_epi8`: we need addition to wrap modulo 255. */
      d = _mm_add_epi8(d, nearest);
      store3(row, _mm_packus_epi16(d, d));

      prev += 3;
      row  += 3;
      rb   -= 3;
   }

   if (rb > 0)
   {
      /* It's easiest to do this math (particularly, deal with pc) with 16-bit
       * intermediates.
       */
      __m128i pa,pb,pc,smallest,nearest;
      c = b; b = _mm_unpacklo_epi8(load3(prev), zero);
      a = d; d = _mm_unpacklo_epi8(load3(row ), zero);

      /* (p-a) == (a+b-c - a) == (b-c) */
      pa = _mm_sub_epi16(b, c);

      /* (p-b) == (a+b-c - b) == (a-c) */
      pb = _mm_sub_epi16(a, c);

      /* (p-c) == (a+b-c - c) == (a+b-c-c) == (b-c)+(a-c) */
      pc = _mm_add_epi16(pa, pb);

      pa = abs_i16(pa);  /* |p-a| */
      pb = abs_i16(pb);  /* |p-b| */
      pc = abs_i16(pc);  /* |p-c| */

      smallest = _mm_min_epi16(pc, _mm_min_epi16(pa, pb));

      /* Paeth breaks ties favoring a over b over c. */
      nearest  = if_then_else(_mm_cmpeq_epi16(smallest, pa), a,
                         if_then_else(_mm_cmpeq_epi16(smallest, pb), b, c));

      /* Note `_epi8`: we need addition to wrap modulo 255. */
      d = _mm_add_epi8(d, nearest);
      store3(row, _mm_packus_epi16(d, d));
   }
}

void png_read_filter_row_paeth4(size_t rowbytes, png_bytep row, png_const_bytep prev)
{
   /* Paeth tries to predict pixel d using the pixel to the left of it, a,
    * and two pixels from the previous row, b and c:
    *   prev: c b
    *   row:  a d
    * The Paeth function predicts d to be whichever of a, b, or c is nearest to
    * p=a+b-c.
    *
    * The first pixel has no left context, and so uses an Up filter, p = b.
    * This works naturally with our main loop's p = a+b-c if we force a and c
    * to zero.
    * Here we zero b and d, which become c and a respectively at the start of
    * the loop.
    */
   size_t rb = rowbytes+4;

   const __m128i zero = _mm_setzero_si128();
   __m128i pa,pb,pc,smallest,nearest;
   __m128i c, b = zero,
           a, d = zero;

   while (rb > 4)
   {
      /* It's easiest to do this math (particularly, deal with pc) with 16-bit
       * intermediates.
       */
      c = b; b = _mm_unpacklo_epi8(load4(prev), zero);
      a = d; d = _mm_unpacklo_epi8(load4(row ), zero);

      /* (p-a) == (a+b-c - a) == (b-c) */
      pa = _mm_sub_epi16(b, c);

      /* (p-b) == (a+b-c - b) == (a-c) */
      pb = _mm_sub_epi16(a, c);

      /* (p-c) == (a+b-c - c) == (a+b-c-c) == (b-c)+(a-c) */
      pc = _mm_add_epi16(pa, pb);

      pa = abs_i16(pa);  /* |p-a| */
      pb = abs_i16(pb);  /* |p-b| */
      pc = abs_i16(pc);  /* |p-c| */

      smallest = _mm_min_epi16(pc, _mm_min_epi16(pa, pb));

      /* Paeth breaks ties favoring a over b over c. */
      nearest  = if_then_else(_mm_cmpeq_epi16(smallest, pa), a,
                         if_then_else(_mm_cmpeq_epi16(smallest, pb), b, c));

      /* Note `_epi8`: we need addition to wrap modulo 255. */
      d = _mm_add_epi8(d, nearest);
      store4(row, _mm_packus_epi16(d, d));

      prev += 4;
      row  += 4;
      rb   -= 4;
   }
}

#endif /* SPNG_OPTIMIZE_FILTER && SPNG_X86 */