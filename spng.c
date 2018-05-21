#include "spng.h"

#include <arpa/inet.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include <zlib.h>

#define SPNG_FILTER_TYPE_NONE 0
#define SPNG_FILTER_TYPE_SUB 1
#define SPNG_FILTER_TYPE_UP 2
#define SPNG_FILTER_TYPE_AVERAGE 3
#define SPNG_FILTER_TYPE_PAETH 4

struct spng_subimage
{
    uint32_t width;
    uint32_t height;
};

struct spng_decomp
{
    unsigned char *buf; /* input buffer */
    size_t size; /* input buffer size */

    char *out; /* output buffer */
    size_t decomp_size; /* decompressed data size */
    size_t decomp_alloc_size; /* actual buffer size */

    size_t initial_size; /* initial value for decomp_size */
};

static const uint32_t png_u32max = 2147483647;

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


/* Used to check if a chunk actually fits in dec->data */
static int require_bytes(size_t offset, size_t bytes, size_t data_size)
{
    size_t required = offset + bytes;
    if(required < bytes) return SPNG_EOVERFLOW;

    if(required > data_size) return SPNG_EOF; /* buffer too small */

    return 0;
}

/* Read and validate the next chunk header */
static int next_header(struct spng_decoder *dec, const struct spng_chunk *current,
                       struct spng_chunk *next)
{
    if(dec == NULL || current == NULL || next == NULL) return 1;

    int ret;
    size_t bytes_required;

    bytes_required = current->length + 20; /* current header, current crc, next header */
    if(bytes_required < current->length) return SPNG_EOVERFLOW;

    if(dec->streaming) ret = dec->read_fn(dec, dec->read_user_ptr, dec->data, 8);
    else ret = require_bytes(current->offset, bytes_required, dec->data_size);

    if(ret) return ret;

    bytes_required -= 8;
    next->offset = current->offset + bytes_required;
    if(next->offset < current->offset) return SPNG_EOVERFLOW;

    if(dec->streaming)
    {
        memcpy(&next->length, dec->data, 4);
        memcpy(&next->type, dec->data + 4, 4);
    }
    else
    {
        memcpy(&next->length, dec->data + next->offset, 4);
        memcpy(&next->type, dec->data + next->offset + 4, 4);
    }

    next->length = ntohl(next->length);

    if(next->length > png_u32max)
    {
        memset(next, 0, sizeof(struct spng_chunk));
        return SPNG_ECHUNK_SIZE;
    }

    return 0;
}

/* Read chunk data when streaming and check crc */
static int get_chunk_data(struct spng_decoder *dec, struct spng_chunk *chunk)
{
    if(dec == NULL || chunk == NULL) return 1;

    int ret;
    size_t bytes_required;

    if(dec->streaming)
    {
        bytes_required = chunk->length + 4;
        if(bytes_required < chunk->length) return SPNG_EOVERFLOW;

        if(bytes_required > dec->data_size)
        {
            void *buf = realloc(dec->data, bytes_required);
            if(buf == NULL) return SPNG_EMEM;

            dec->data = buf;
            dec->data_size = bytes_required;
        }

        ret = dec->read_fn(dec, dec->read_user_ptr, dec->data, bytes_required);
    }
    else
    {
        bytes_required = chunk->length + 12; /* header, crc */
        if(bytes_required < chunk->length) return SPNG_EOVERFLOW;

        ret = require_bytes(chunk->offset, bytes_required, dec->data_size);
    }

    if(ret) return ret;

    uint32_t actual_crc = crc32(0, Z_NULL, 0);
    actual_crc = crc32(actual_crc, chunk->type, 4);

    if(dec->streaming)
    {
        actual_crc = crc32(actual_crc, dec->data, chunk->length);
        memcpy(&chunk->crc, dec->data + chunk->length, 4);
    }
    else
    {
        actual_crc = crc32(actual_crc, dec->data + chunk->offset + 8, chunk->length);
        memcpy(&chunk->crc, dec->data + chunk->offset + 8 + chunk->length, 4);
    }

    chunk->crc = ntohl(chunk->crc);
    if(actual_crc != chunk->crc) return SPNG_ECHUNK_CRC;

    return 0;
}

static int check_ihdr(struct spng_ihdr *ihdr)
{
    if(ihdr->width > png_u32max) return SPNG_EWIDTH;
    if(ihdr->height > png_u32max) return SPNG_EHEIGHT;

    switch(ihdr->colour_type)
    {
        case SPNG_COLOUR_TYPE_GRAYSCALE:
        {
            if( !(ihdr->bit_depth == 1 || ihdr->bit_depth == 2 ||
                  ihdr->bit_depth == 4 || ihdr->bit_depth == 8 ||
                  ihdr->bit_depth == 16) )
                  return SPNG_EBIT_DEPTH;

            break;
        }
        case SPNG_COLOUR_TYPE_TRUECOLOR:
        {
            if( !(ihdr->bit_depth == 8 || ihdr->bit_depth == 16) )
                return SPNG_EBIT_DEPTH;

            break;
        }
        case SPNG_COLOUR_TYPE_INDEXED:
        {
            if( !(ihdr->bit_depth == 1 || ihdr->bit_depth == 2 ||
                  ihdr->bit_depth == 4 || ihdr->bit_depth == 8) )
                return SPNG_EBIT_DEPTH;

            break;
        }
        case SPNG_COLOUR_TYPE_GRAYSCALE_ALPHA:
        {
            if( !(ihdr->bit_depth == 8 || ihdr->bit_depth == 16) )
                return SPNG_EBIT_DEPTH;

            break;
        }
        case SPNG_COLOUR_TYPE_TRUECOLOR_ALPHA:
        {
            if( !(ihdr->bit_depth == 8 || ihdr->bit_depth == 16) )
                return SPNG_EBIT_DEPTH;

            break;
        }
    default: return SPNG_ECOLOUR_TYPE;
    }

    if(ihdr->compression_method || ihdr->filter_method)
        return SPNG_ECOMPRESSION_METHOD;

    if( !(ihdr->interlace_method == 0 || ihdr->interlace_method == 1) )
        return SPNG_EINTERLACE_METHOD;

    return 0;
}

/* Validate PNG keyword *str, *str must be 80 bytes */
static int check_png_keyword(const char str[80])
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

        if( (c >= 32 && c <= 126) || (c >= 161 && c <= 255) ) str++;
        else return 1; /* invalid character */
    }

    return 0;
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

static int defilter_scanline(const unsigned char *prev_scanline, unsigned char *scanline,
                             size_t scanline_width, uint8_t bytes_per_pixel)
{
    if(prev_scanline==NULL || scanline==NULL || scanline_width <= 1) return 1;

    uint8_t filter = 0;
    memcpy(&filter, scanline, 1);

    if(filter > 4) return SPNG_EFILTER;
    if(filter == 0) return 0;

    size_t i;
    for(i=1; i < scanline_width; i++)
    {
        uint8_t x, a, b, c;

        if(i > bytes_per_pixel)
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

/* Decompress a zlib stream from params->buf to params->out,
   params->out is allocated by the function,
   params->size and initial_size should be non-zero. */
static int decompress_zstream(struct spng_decomp *params)
{
    if(params == NULL) return 1;
    if(params->buf == NULL || params->size == 0) return 1;
    if(params->initial_size == 0) return 1;

    params->decomp_alloc_size = params->initial_size;
    params->out = malloc(params->decomp_alloc_size);
    if(params->out == NULL) return 1;

    int ret;
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    if(inflateInit(&stream) != Z_OK)
    {
        free(params->out);
        return SPNG_EZLIB;
    }

    stream.avail_in = params->size;
    stream.next_in = params->buf;

    stream.avail_out = params->decomp_alloc_size;
    stream.next_out = (unsigned char*)params->out;

    do
    {
        ret = inflate(&stream, Z_SYNC_FLUSH);

        if(ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR)
        {
            inflateEnd(&stream);
            free(params->out);
            return SPNG_EZLIB;
        }

        if(stream.avail_out == 0 && stream.avail_in != 0)
        {
            if(2 > SIZE_MAX / params->decomp_alloc_size)
            {
                inflateEnd(&stream);
                free(params->out);
                return SPNG_EOVERFLOW;
            }

            params->decomp_alloc_size *= 2;
            void *temp = realloc(params->out, params->decomp_alloc_size);

            if(temp == NULL)
            {
                inflateEnd(&stream);
                free(params->out);
                return SPNG_EMEM;
            }

            params->out = temp;

            /* doubling the buffer size means its now half full */
            stream.avail_out = params->decomp_alloc_size / 2;
            stream.next_out = (unsigned char*)params->out + params->decomp_alloc_size / 2;
        }

    }while(ret != Z_STREAM_END);

    params->decomp_size = stream.total_out;
    inflateEnd(&stream);

    return 0;
}

/*
    Read and validate all critical and relevant ancillary chunks up to the first IDAT
    Returns zero and sets dec->first_idat on success
*/
static int get_ancillary_data_first_idat(struct spng_decoder *dec)
{
    if(dec==NULL) return 1;
    if(dec->data == NULL) return 1;
    if(!dec->valid_state) return SPNG_EBADSTATE;

    int ret;
    unsigned char *data;
    struct spng_chunk chunk, next;

    chunk.offset = 8;
    chunk.length = 13;
    size_t sizeof_sig_ihdr = 33;

    if(dec->streaming)
    {
        ret = dec->read_fn(dec, dec->read_user_ptr, dec->data, sizeof_sig_ihdr);
        if(ret) return ret;
    }

    data = dec->data;

    uint8_t signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if(memcmp(data, signature, sizeof(signature))) return SPNG_ESIGNATURE;

    memcpy(&chunk.length, data + 8, 4);
    memcpy(&chunk.type, data + 12, 4);
    memcpy(&chunk.crc, data + 29, 4);

    chunk.length = ntohl(chunk.length);
    chunk.crc = ntohl(chunk.crc);

    uint32_t actual_crc = crc32(0, Z_NULL, 0);
    actual_crc = crc32(actual_crc, dec->data + 12, 17);
    if(actual_crc != chunk.crc) return SPNG_ECHUNK_CRC;

    if(chunk.length != 13) return SPNG_EIHDR_SIZE;
    if(memcmp(chunk.type, type_ihdr, 4)) return SPNG_ENOIHDR;

    memcpy(&dec->ihdr.width, data + 16, 4);
    memcpy(&dec->ihdr.height, data + 20, 4);
    memcpy(&dec->ihdr.bit_depth, data + 24, 1);
    memcpy(&dec->ihdr.colour_type, data + 25, 1);
    memcpy(&dec->ihdr.compression_method, data + 26, 1);
    memcpy(&dec->ihdr.filter_method, data + 27, 1);
    memcpy(&dec->ihdr.interlace_method, data + 28, 1);

    dec->ihdr.width = ntohl(dec->ihdr.width);
    dec->ihdr.height = ntohl(dec->ihdr.height);

    ret = check_ihdr(&dec->ihdr);
    if(ret) return ret;

    dec->have_ihdr = 1;

    while( !(ret = next_header(dec, &chunk, &next)))
    {
        memcpy(&chunk, &next, sizeof(struct spng_chunk));

        if(!memcmp(chunk.type, type_idat, 4))
        {
            memcpy(&dec->first_idat, &chunk, sizeof(struct spng_chunk));
            return 0;
        }

        ret = get_chunk_data(dec, &chunk);
        if(ret) return ret;

        if(dec->streaming) data = dec->data;
        else data = dec->data + chunk.offset + 8;

        /* Reserved bit must be zero */
        if( (chunk.type[2] & (1 << 5)) != 0) return SPNG_ECHUNK_TYPE;
        /* Ignore private chunks */
        if( (chunk.type[1] & (1 << 5)) != 0) continue;

        if( (chunk.type[0] & (1 << 5)) == 0) /* Critical chunk */
        {
            if(!memcmp(chunk.type, type_plte, 4))
            {
                if(chunk.length == 0) return SPNG_ECHUNK_SIZE;
                if(chunk.length % 3 != 0) return SPNG_ECHUNK_SIZE;
                if( (chunk.length / 3) > 256 ) return SPNG_ECHUNK_SIZE;

                if(dec->ihdr.colour_type == 3)
                {
                    if(chunk.length / 3 > (1 << dec->ihdr.bit_depth) ) return SPNG_ECHUNK_SIZE;
                }

                dec->plte.n_entries = chunk.length / 3;

                size_t i;
                for(i=0; i < dec->plte.n_entries; i++)
                {
                    memcpy(&dec->plte.entries[i].red,   data + i * 3, 1);
                    memcpy(&dec->plte.entries[i].green, data + i * 3 + 1, 1);
                    memcpy(&dec->plte.entries[i].blue,  data + i * 3 + 2, 1);
                }

                dec->plte_offset = chunk.offset;

                dec->have_plte = 1;
            }
            else if(!memcmp(chunk.type, type_iend, 4)) return SPNG_ECHUNK_POS;
            else if(!memcmp(chunk.type, type_ihdr, 4)) return SPNG_ECHUNK_POS;
            else return SPNG_ECHUNK_UNKNOWN_CRITICAL;
        }
        else if(!memcmp(chunk.type, type_chrm, 4)) /* Ancillary chunks */
        {
            if(dec->have_plte && chunk.offset > dec->plte_offset) return SPNG_ECHUNK_POS;
            if(dec->have_chrm) return SPNG_EDUP_CHRM;

            if(chunk.length != 32) return SPNG_ECHUNK_SIZE;

            memcpy(&dec->chrm.white_point_x, data, 4);
            memcpy(&dec->chrm.white_point_y, data + 4, 4);
            memcpy(&dec->chrm.red_x, data + 8, 4);
            memcpy(&dec->chrm.red_y, data + 12, 4);
            memcpy(&dec->chrm.green_x, data + 16, 4);
            memcpy(&dec->chrm.green_y, data + 20, 4);
            memcpy(&dec->chrm.blue_x, data + 24, 4);
            memcpy(&dec->chrm.blue_y, data + 28, 4);

            dec->chrm.white_point_x = ntohl(dec->chrm.white_point_x);
            dec->chrm.white_point_y = ntohl(dec->chrm.white_point_y);
            dec->chrm.red_x = ntohl(dec->chrm.red_x);
            dec->chrm.red_y = ntohl(dec->chrm.red_y);
            dec->chrm.green_x = ntohl(dec->chrm.green_x);
            dec->chrm.green_y = ntohl(dec->chrm.green_y);
            dec->chrm.blue_x = ntohl(dec->chrm.blue_x);
            dec->chrm.blue_y = ntohl(dec->chrm.blue_y);

            if(dec->chrm.white_point_x > png_u32max ||
               dec->chrm.white_point_y > png_u32max ||
               dec->chrm.red_x > png_u32max ||
               dec->chrm.red_y > png_u32max ||
               dec->chrm.green_x  > png_u32max ||
               dec->chrm.green_y  > png_u32max ||
               dec->chrm.blue_x > png_u32max ||
               dec->chrm.blue_y > png_u32max) return SPNG_ECHRM;

            dec->have_chrm = 1;
        }
        else if(!memcmp(chunk.type, type_gama, 4))
        {
            if(dec->have_plte && chunk.offset > dec->plte_offset) return SPNG_ECHUNK_POS;
            if(dec->have_gama) return SPNG_EDUP_GAMA;

            if(chunk.length != 4) return SPNG_ECHUNK_SIZE;

            memcpy(&dec->gama, data, 4);

            dec->gama = ntohl(dec->gama);

            if(dec->gama > png_u32max) return SPNG_EGAMA;

            dec->have_gama = 1;
        }
        else if(!memcmp(chunk.type, type_iccp, 4))
        {
            if(dec->have_plte && chunk.offset > dec->plte_offset) return SPNG_ECHUNK_POS;
            if(dec->have_iccp) return SPNG_EDUP_ICCP;

            size_t name_len = chunk.length > 80 ? 80 : chunk.length;
            char *name_nul = memchr(data, '\0', name_len);
            if(name_nul == NULL) return SPNG_EICCP_NAME;

            memcpy(&dec->iccp.profile_name, data, name_len);

            if(check_png_keyword(dec->iccp.profile_name)) return SPNG_EICCP_NAME;

            name_len = strlen(dec->iccp.profile_name);

            /* check for at least 2 bytes after profile name */
            if( (chunk.length - name_len - 1) < 2) return SPNG_ECHUNK_SIZE;

            uint8_t compression_method;
            memcpy(&compression_method, data + name_len + 1, 1);
            if(compression_method) return SPNG_EICCP_COMPRESSION_METHOD;

            struct spng_decomp params;
            params.buf = data + name_len + 2;
            params.size = chunk.length - name_len - 2;
            params.initial_size = 1024;

            ret = decompress_zstream(&params);
            if(ret) return ret;

            dec->iccp.profile = params.out;
            dec->iccp.profile_len = params.decomp_size;

            dec->have_iccp = 1;
        }
        else if(!memcmp(chunk.type, type_sbit, 4))
        {
            if(dec->have_plte && chunk.offset > dec->plte_offset) return SPNG_ECHUNK_POS;
            if(dec->have_sbit) return SPNG_EDUP_SBIT;

            if(dec->ihdr.colour_type == 0)
            {
                if(chunk.length != 1) return SPNG_ECHUNK_SIZE;

                memcpy(&dec->sbit.greyscale_bits, data, 1);

                if(dec->sbit.greyscale_bits == 0) return SPNG_ESBIT;
                if(dec->sbit.greyscale_bits > dec->ihdr.bit_depth) return SPNG_ESBIT;
            }
            else if(dec->ihdr.colour_type == 2 || dec->ihdr.colour_type == 3)
            {
                if(chunk.length != 3) return SPNG_ECHUNK_SIZE;

                memcpy(&dec->sbit.red_bits, data, 1);
                memcpy(&dec->sbit.green_bits, data + 1 , 1);
                memcpy(&dec->sbit.blue_bits, data + 2, 1);

                if(dec->sbit.red_bits == 0) return SPNG_ESBIT;
                if(dec->sbit.green_bits == 0) return SPNG_ESBIT;
                if(dec->sbit.blue_bits == 0) return SPNG_ESBIT;

                uint8_t bit_depth;
                if(dec->ihdr.colour_type == 3) bit_depth = 8;
                else bit_depth = dec->ihdr.bit_depth;

                if(dec->sbit.red_bits > bit_depth) return SPNG_ESBIT;
                if(dec->sbit.green_bits > bit_depth) return SPNG_ESBIT;
                if(dec->sbit.blue_bits > bit_depth) return SPNG_ESBIT;
            }
            else if(dec->ihdr.colour_type == 4)
            {
                if(chunk.length != 2) return SPNG_ECHUNK_SIZE;

                memcpy(&dec->sbit.greyscale_bits, data, 1);
                memcpy(&dec->sbit.alpha_bits, data + 1, 1);

                if(dec->sbit.greyscale_bits == 0) return SPNG_ESBIT;
                if(dec->sbit.greyscale_bits > dec->ihdr.bit_depth) return SPNG_ESBIT;

                if(dec->sbit.alpha_bits == 0) return SPNG_ESBIT;
                if(dec->sbit.alpha_bits > dec->ihdr.bit_depth) return SPNG_ESBIT;
            }
            else if(dec->ihdr.colour_type == 6)
            {
                if(chunk.length != 4) return SPNG_ECHUNK_SIZE;

                memcpy(&dec->sbit.red_bits, data, 1);
                memcpy(&dec->sbit.green_bits, data + 1, 1);
                memcpy(&dec->sbit.blue_bits, data + 2, 1);
                memcpy(&dec->sbit.alpha_bits, data + 3, 1);

                if(dec->sbit.red_bits == 0) return SPNG_ESBIT;
                if(dec->sbit.green_bits == 0) return SPNG_ESBIT;
                if(dec->sbit.blue_bits == 0) return SPNG_ESBIT;
                if(dec->sbit.alpha_bits == 0) return SPNG_ESBIT;

                if(dec->sbit.red_bits > dec->ihdr.bit_depth) return SPNG_ESBIT;
                if(dec->sbit.green_bits > dec->ihdr.bit_depth) return SPNG_ESBIT;
                if(dec->sbit.blue_bits > dec->ihdr.bit_depth) return SPNG_ESBIT;
                if(dec->sbit.alpha_bits > dec->ihdr.bit_depth) return SPNG_ESBIT;
            }

            dec->have_sbit = 1;
        }
        else if(!memcmp(chunk.type, type_srgb, 4))
        {
            if(dec->have_plte && chunk.offset > dec->plte_offset) return SPNG_ECHUNK_POS;
            if(dec->have_srgb) return SPNG_EDUP_SRGB;

            if(chunk.length != 1) return SPNG_ECHUNK_SIZE;

            memcpy(&dec->srgb_rendering_intent, data, 1);

            dec->have_srgb = 1;
        }
        else if(!memcmp(chunk.type, type_bkgd, 4))
        {
            if(dec->have_plte && chunk.offset < dec->plte_offset) return SPNG_ECHUNK_POS;
            if(dec->have_bkgd) return SPNG_EDUP_BKGD;

            uint16_t mask = ~0;
            if(dec->ihdr.bit_depth < 16) mask = (1 << dec->ihdr.bit_depth) - 1;

            if(dec->ihdr.colour_type == 0 || dec->ihdr.colour_type == 4)
            {
                if(chunk.length != 2) return SPNG_ECHUNK_SIZE;

                memcpy(&dec->bkgd.type0_4_greyscale, data, 2);

                dec->bkgd.type0_4_greyscale = ntohs(dec->bkgd.type0_4_greyscale) & mask;
            }
            else if(dec->ihdr.colour_type == 2 || dec->ihdr.colour_type == 6)
            {
                if(chunk.length != 6) return SPNG_ECHUNK_SIZE;

                memcpy(&dec->bkgd.type2_6.red, data, 2);
                memcpy(&dec->bkgd.type2_6.green, data + 2, 2);
                memcpy(&dec->bkgd.type2_6.blue, data + 4, 2);

                dec->bkgd.type2_6.red = ntohs(dec->bkgd.type2_6.red) & mask;
                dec->bkgd.type2_6.green = ntohs(dec->bkgd.type2_6.green) & mask;
                dec->bkgd.type2_6.blue = ntohs(dec->bkgd.type2_6.blue) & mask;
            }
            else if(dec->ihdr.colour_type == 3)
            {
                if(chunk.length != 1) return SPNG_ECHUNK_SIZE;
                if(!dec->have_plte) return SPNG_EBKGD_NO_PLTE;

                memcpy(&dec->bkgd.type3_plte_index, data, 1);
                if(dec->bkgd.type3_plte_index >= dec->plte.n_entries) return SPNG_EBKGD_PLTE_IDX;
            }

            dec->have_bkgd = 1;
        }
        else if(!memcmp(chunk.type, type_trns, 4))
        {
            if(dec->have_plte && chunk.offset < dec->plte_offset) return SPNG_ECHUNK_POS;
            if(dec->have_trns) return SPNG_EDUP_TRNS;
            if(!chunk.length) return SPNG_ECHUNK_SIZE;

            uint16_t mask = ~0;
            if(dec->ihdr.bit_depth < 16) mask = (1 << dec->ihdr.bit_depth) - 1;

            if(dec->ihdr.colour_type == 0)
            {
                if(chunk.length != 2) return SPNG_ECHUNK_SIZE;

                memcpy(&dec->trns.type0_grey_sample, data, 2);

                dec->trns.type0_grey_sample = ntohs(dec->trns.type0_grey_sample) & mask;
            }
            else if(dec->ihdr.colour_type == 2)
            {
                if(chunk.length != 6) return SPNG_ECHUNK_SIZE;

                memcpy(&dec->trns.type2.red, data, 2);
                memcpy(&dec->trns.type2.green, data + 2, 2);
                memcpy(&dec->trns.type2.blue, data + 4, 2);

                dec->trns.type2.red = ntohs(dec->trns.type2.red) & mask;
                dec->trns.type2.green = ntohs(dec->trns.type2.green) & mask;
                dec->trns.type2.blue = ntohs(dec->trns.type2.blue) & mask;
            }
            else if(dec->ihdr.colour_type == 3)
            {
                if(chunk.length > dec->plte.n_entries) return SPNG_ECHUNK_SIZE;
                if(!dec->have_plte) return SPNG_ETRNS_NO_PLTE;

                size_t k;
                for(k=0; k < chunk.length; k++)
                {
                    memcpy(&dec->trns.type3_alpha[k], data + k, 1);
                }
                dec->trns.n_type3_entries = chunk.length;
            }
            else return SPNG_ETRNS_COLOUR_TYPE;

            dec->have_trns = 1;
        }
        else if(!memcmp(chunk.type, type_hist, 4))
        {
            if(!dec->have_plte) return SPNG_EHIST_NO_PLTE;
            if(chunk.offset < dec->plte_offset) return SPNG_ECHUNK_POS;
            if(dec->have_hist) return SPNG_EDUP_HIST;

            if( (chunk.length / 2) != (dec->plte.n_entries) ) return SPNG_ECHUNK_SIZE;

            size_t k;
            for(k=0; k < (chunk.length / 2); k++)
            {
                memcpy(&dec->hist.frequency[k], data + k*2, 2);

                dec->hist.frequency[k] = ntohs(dec->hist.frequency[k]);
            }

            dec->have_hist = 1;
        }
        else if(!memcmp(chunk.type, type_phys, 4))
        {
            if(dec->have_phys) return SPNG_EDUP_PHYS;

            if(chunk.length != 9) return SPNG_ECHUNK_SIZE;

            memcpy(&dec->phys.ppu_x, data, 4);
            memcpy(&dec->phys.ppu_y, data + 4, 4);
            memcpy(&dec->phys.unit_specifier, data + 8, 1);

            dec->phys.ppu_x = ntohl(dec->phys.ppu_x);
            dec->phys.ppu_y = ntohl(dec->phys.ppu_y);

            if(dec->phys.unit_specifier > 1) return SPNG_EPHYS;

            if(dec->phys.ppu_x > png_u32max) return SPNG_EPHYS;
            if(dec->phys.ppu_y > png_u32max) return SPNG_EPHYS;

            dec->have_phys = 1;
        }
        else if(!memcmp(chunk.type, type_splt, 4))
        {
            if(!dec->have_splt)
            {
                dec->n_splt = 1;
                dec->splt_list = malloc(sizeof(struct spng_splt));
                if(dec->splt_list == NULL) return SPNG_EMEM;
            }
            else
            {
                dec->n_splt++;
                if(dec->n_splt < 1) return SPNG_EOVERFLOW;
                if(dec->n_splt > SIZE_MAX / sizeof(struct spng_splt)) return SPNG_EOVERFLOW;

                void *buf = realloc(dec->splt_list, dec->n_splt * sizeof(struct spng_splt));
                if(buf == NULL) return SPNG_EMEM;
                dec->splt_list = buf;
            }

            uint32_t i = dec->n_splt - 1;

            size_t keyword_len = chunk.length > 80 ? 80 : chunk.length;
            char *keyword_nul = memchr(data, '\0', keyword_len);
            if(keyword_nul == NULL) return SPNG_ESPLT_NAME;

            memcpy(&dec->splt_list[i].name, data, keyword_len);

            if(check_png_keyword(dec->splt_list[i].name)) return SPNG_ESPLT_NAME;

            keyword_len = strlen(dec->splt_list[i].name);

            if( (chunk.length - keyword_len - 1) ==  0) return SPNG_ECHUNK_SIZE;

            memcpy(&dec->splt_list[i].sample_depth, data + keyword_len + 1, 1);

            if(dec->n_splt > 1)
            {
                uint32_t j;
                for(j=0; j < i; j++)
                {
                    if(!strcmp(dec->splt_list[j].name, dec->splt_list[i].name)) return SPNG_ESPLT_DUP_NAME;
                }
            }

            if(dec->splt_list[i].sample_depth == 16)
            {
                if( (chunk.length - keyword_len - 2) % 10 != 0) return SPNG_ECHUNK_SIZE;
                dec->splt_list[i].n_entries = (chunk.length - keyword_len - 2) / 10;
            }
            else if(dec->splt_list[i].sample_depth == 8)
            {
                if( (chunk.length - keyword_len - 2) % 6 != 0) return SPNG_ECHUNK_SIZE;
                dec->splt_list[i].n_entries = (chunk.length - keyword_len - 2) / 6;
            }
            else return SPNG_ESPLT_DEPTH;

            if(dec->splt_list[i].n_entries == 0) return SPNG_ECHUNK_SIZE;
            if(sizeof(struct spng_splt_entry) > SIZE_MAX / dec->splt_list[i].n_entries) return SPNG_EOVERFLOW;

            dec->splt_list[i].entries = malloc(sizeof(struct spng_splt_entry) * dec->splt_list[i].n_entries);
            if(dec->splt_list[i].entries == NULL) return SPNG_EMEM;

            const unsigned char *splt = data + keyword_len + 2;

            size_t k;
            if(dec->splt_list[i].sample_depth == 16)
            {
                for(k=0; k < dec->splt_list[i].n_entries; k++)
                {
                    memcpy(&dec->splt_list[i].entries[k].red,   splt + k * 10, 2);
                    memcpy(&dec->splt_list[i].entries[k].green, splt + k * 10 + 2, 2);
                    memcpy(&dec->splt_list[i].entries[k].blue,  splt + k * 10 + 4, 2);
                    memcpy(&dec->splt_list[i].entries[k].alpha, splt + k * 10 + 6, 2);
                    memcpy(&dec->splt_list[i].entries[k].frequency, splt + k * 10 + 8, 2);

                    dec->splt_list[i].entries[k].red = ntohs(dec->splt_list[i].entries[k].red);
                    dec->splt_list[i].entries[k].green = ntohs(dec->splt_list[i].entries[k].green);
                    dec->splt_list[i].entries[k].blue = ntohs(dec->splt_list[i].entries[k].blue);
                    dec->splt_list[i].entries[k].alpha = ntohs(dec->splt_list[i].entries[k].alpha);
                    dec->splt_list[i].entries[k].frequency = ntohs(dec->splt_list[i].entries[k].frequency);
                }
            }
            else if(dec->splt_list[i].sample_depth == 8)
            {
                for(k=0; k < dec->splt_list[i].n_entries; k++)
                {
                    uint8_t red, green, blue, alpha;
                    memcpy(&red,   splt + k * 6, 1);
                    memcpy(&green, splt + k * 6 + 1, 1);
                    memcpy(&blue,  splt + k * 6 + 2, 1);
                    memcpy(&alpha, splt + k * 6 + 3, 1);
                    memcpy(&dec->splt_list[i].entries[k].frequency, splt + k * 6 + 4, 2);

                    dec->splt_list[i].entries[k].red = red;
                    dec->splt_list[i].entries[k].green = green;
                    dec->splt_list[i].entries[k].blue = blue;
                    dec->splt_list[i].entries[k].alpha = alpha;
                    dec->splt_list[i].entries[k].frequency = ntohs(dec->splt_list[i].entries[k].frequency);
                }
            }

            dec->have_splt = 1;
        }
        else if(!memcmp(chunk.type, type_time, 4))
        {
            if(dec->have_time) return SPNG_EDUP_TIME;

            if(chunk.length != 7) return SPNG_ECHUNK_SIZE;

            memcpy(&dec->time.year, data, 2);
            memcpy(&dec->time.month, data + 2, 1);
            memcpy(&dec->time.day, data + 3, 1);
            memcpy(&dec->time.hour, data + 4, 1);
            memcpy(&dec->time.minute, data + 5, 1);
            memcpy(&dec->time.second, data + 6, 1);

            dec->time.year = ntohs(dec->time.year);

            if(dec->time.month == 0 || dec->time.month > 12) return SPNG_ETIME;
            if(dec->time.day == 0 || dec->time.day > 31) return SPNG_ETIME;
            if(dec->time.hour > 23) return SPNG_ETIME;
            if(dec->time.minute > 59) return SPNG_ETIME;
            if(dec->time.second > 60) return SPNG_ETIME;

            dec->have_time = 1;
        }
        else if(!memcmp(chunk.type, type_text, 4))
        {/* TODO: read */
        }
        else if(!memcmp(chunk.type, type_ztxt, 4))
        {/* TODO: read */
        }
        else if(!memcmp(chunk.type, type_itxt, 4))
        {/* TODO: read */
        }
    }

    return ret;
}

static int validate_past_idat(struct spng_decoder *dec)
{
    if(dec == NULL) return 1;

    int ret;
    int prev_was_idat = 1;
    struct spng_chunk chunk, next;

    memcpy(&chunk, &dec->last_idat, sizeof(struct spng_chunk));

    while( !(ret = next_header(dec, &chunk, &next)))
    {
        memcpy(&chunk, &next, sizeof(struct spng_chunk));

        ret = get_chunk_data(dec, &chunk);
        if(ret) return ret;

        /* Reserved bit must be zero */
        if( (chunk.type[2] & (1 << 5)) != 0) return SPNG_ECHUNK_TYPE;
         /* Ignore private chunks */
        if( (chunk.type[1] & (1 << 5)) != 0) continue;

        /* Critical chunk */
        if( (chunk.type[0] & (1 << 5)) == 0)
        {
            if(!memcmp(chunk.type, type_iend, 4)) return 0;
            else if(!memcmp(chunk.type, type_idat, 4) && prev_was_idat) continue; /* ignore extra IDATs */
            else return SPNG_ECHUNK_POS; /* critical chunk after last IDAT that isn't IEND */
        }

        prev_was_idat = 0;

        if(!memcmp(chunk.type, type_chrm, 4)) return SPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_gama, 4)) return SPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_iccp, 4)) return SPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_sbit, 4)) return SPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_srgb, 4)) return SPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_bkgd, 4)) return SPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_hist, 4)) return SPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_trns, 4)) return SPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_phys, 4)) return SPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_splt, 4)) return SPNG_ECHUNK_POS;
    }

    return ret;
}


/* Scale "sbits" significant bits in "sample" from "bit_depth" to "target"

   "bit_depth" must be a valid PNG depth
   "sbits" must be less than or equal to "bit_depth"
   "target" must be between 1 and 16
*/
static uint16_t sample_to_target(uint16_t sample, uint8_t bit_depth, uint8_t sbits, uint8_t target)
{
    uint16_t sample_bits;
    int8_t shift_amount;

    if(bit_depth == sbits)
    {
        if(target == sbits) return sample; /* no scaling */
    }/* bit_depth > sbits */
    else sample = sample >> (bit_depth - sbits); /* shift significant bits to bottom */

    /* downscale */
    if(target < sbits) return sample >> (sbits - target);

    /* upscale using left bit replication */
    shift_amount = target - sbits;
    sample_bits = sample;
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


struct spng_decoder * spng_decoder_new(void)
{
    struct spng_decoder *dec = malloc(sizeof(struct spng_decoder));
    if(dec==NULL) return NULL;

    memset(dec, 0, sizeof(struct spng_decoder));
    dec->valid_state = 1;

    return dec;
}

void spng_decoder_free(struct spng_decoder *dec)
{
    if(dec==NULL) return;

    if(dec->streaming)
    {
        if(dec->data != NULL) free(dec->data);
    }

    if(dec->iccp.profile != NULL) free(dec->iccp.profile);

    if(dec->gamma_lut != NULL) free(dec->gamma_lut);

    if(dec->splt_list != NULL)
    {
        uint32_t i;
        for(i=0; i < dec->n_splt; i++)
        {
            if(dec->splt_list[i].entries != NULL) free(dec->splt_list[i].entries);
        }
        free(dec->splt_list);
    }

    memset(dec, 0, sizeof(struct spng_decoder));

    free(dec);
}

int spng_decoder_set_buffer(struct spng_decoder *dec, void *buf, size_t size)
{
    if(dec==NULL || buf==NULL) return 1;
    if(!dec->valid_state) return SPNG_EBADSTATE;

    if(dec->data != NULL) return SPNG_EBUF_SET;

    dec->data = buf;
    dec->data_size = size;

    return 0;
}

int spng_decoder_set_stream(struct spng_decoder *dec, spng_read_fn read_func, void *user)
{
    if(dec == NULL || read_func == NULL) return 1;
    if(!dec->valid_state) return SPNG_EBADSTATE;

    if(dec->data != NULL) return SPNG_EBUF_SET;

    dec->data = malloc(8192);
    if(dec->data == NULL) return SPNG_EMEM;
    dec->data_size = 8192;

    dec->read_fn = read_func;
    dec->read_user_ptr = user;

    dec->streaming = 1;

    return 0;
}

static int get_ancillary(struct spng_decoder *dec)
{
    if(dec == NULL) return 1;
    if(dec->data == NULL) return 1;
    if(!dec->valid_state) return SPNG_EBADSTATE;

    int ret;
    if(!dec->first_idat.offset)
    {
        ret = get_ancillary_data_first_idat(dec);
        if(ret)
        {
            dec->valid_state = 0;
            return ret;
        }
    }

    return 0;
}

int spng_get_ihdr(struct spng_decoder *dec, struct spng_ihdr *ihdr)
{
    if(ihdr==NULL) return 1;

    int ret = get_ancillary(dec);
    if(ret) return ret;

    memcpy(ihdr, &dec->ihdr, sizeof(struct spng_ihdr));

    return 0;
}

int spng_get_output_image_size(struct spng_decoder *dec, int fmt, size_t *out)
{
    if(dec==NULL || out==NULL) return 1;

    if(!dec->valid_state) return SPNG_EBADSTATE;

    int ret = get_ancillary(dec);
    if(ret) return ret;

    size_t res;
    if(fmt == SPNG_FMT_RGBA8)
    {
        if(4 > SIZE_MAX / dec->ihdr.width) return SPNG_EOVERFLOW;
        res = 4 * dec->ihdr.width;

        if(res > SIZE_MAX / dec->ihdr.height) return SPNG_EOVERFLOW;
        res = res * dec->ihdr.height;
    }
    else if(fmt == SPNG_FMT_RGBA16)
    {
        if(8 > SIZE_MAX / dec->ihdr.width) return SPNG_EOVERFLOW;
        res = 8 * dec->ihdr.width;

        if(res > SIZE_MAX / dec->ihdr.height) return SPNG_EOVERFLOW;
        res = res * dec->ihdr.height;
    }
    else return SPNG_EFMT;

    *out = res;

    return 0;
}

int spng_decode_image(struct spng_decoder *dec, int fmt, unsigned char *out, size_t out_size, int flags)
{
    if(dec==NULL) return 1;
    if(out==NULL) return 1;
    if(!dec->valid_state) return SPNG_EBADSTATE;

    int ret;
    size_t out_size_required;

    ret = spng_get_output_image_size(dec, fmt, &out_size_required);
    if(ret) return ret;
    if(out_size < out_size_required) return SPNG_EBUFSIZ;

    ret = get_ancillary(dec);
    if(ret) return ret;

    uint8_t channels = 1; /* grayscale or indexed_colour */

    if(dec->ihdr.colour_type == SPNG_COLOUR_TYPE_TRUECOLOR) channels = 3;
    else if(dec->ihdr.colour_type == SPNG_COLOUR_TYPE_GRAYSCALE_ALPHA) channels = 2;
    else if(dec->ihdr.colour_type == SPNG_COLOUR_TYPE_TRUECOLOR_ALPHA) channels = 4;

    uint8_t bytes_per_pixel;

    if(dec->ihdr.bit_depth < 8) bytes_per_pixel = 1;
    else bytes_per_pixel = channels * (dec->ihdr.bit_depth / 8);

    /* Calculate scanline width in bits, round up to a multiple of 8, convert to bytes */
    size_t scanline_width = dec->ihdr.width;

    if(scanline_width > SIZE_MAX / channels) return SPNG_EOVERFLOW;
    scanline_width = scanline_width * channels;

    if(scanline_width > SIZE_MAX / dec->ihdr.bit_depth) return SPNG_EOVERFLOW;
    scanline_width = scanline_width * dec->ihdr.bit_depth;

    scanline_width = scanline_width + 8; /* filter byte */
    if(scanline_width < 8) return SPNG_EOVERFLOW;

    if(scanline_width % 8 != 0) /* round to up multiple of 8 */
    {
        scanline_width = scanline_width + 8;
        if(scanline_width < 8) return SPNG_EOVERFLOW;
        scanline_width = scanline_width - (scanline_width % 8);
    }

    scanline_width = scanline_width / 8;


    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    if(inflateInit(&stream) != Z_OK) return SPNG_EZLIB;

    unsigned char *scanline_orig, *scanline, *prev_scanline;

    scanline_orig = malloc(scanline_width);
    if(scanline_orig==NULL)
    {
        inflateEnd(&stream);
        return SPNG_EMEM;
    }

    /* Some of the error-handling goto's might leave scanline incremented,
       leading to a failed free(), this prevents that. */
    scanline = scanline_orig;

    prev_scanline = malloc(scanline_width);
    if(prev_scanline==NULL)
    {
        inflateEnd(&stream);
        free(scanline_orig);
        return SPNG_EMEM;
    }

    struct spng_subimage sub[7];
    memset(sub, 0, sizeof(struct spng_subimage) * 7);

    if(dec->ihdr.interlace_method == 1)
    {
        sub[0].width = (dec->ihdr.width + 7) >> 3;
        sub[0].height = (dec->ihdr.height + 7) >> 3;
        sub[1].width = (dec->ihdr.width + 3) >> 3;
        sub[1].height = (dec->ihdr.height + 7) >> 3;
        sub[2].width = (dec->ihdr.width + 3) >> 2;
        sub[2].height = (dec->ihdr.height + 3) >> 3;
        sub[3].width = (dec->ihdr.width + 1) >> 2;
        sub[3].height = (dec->ihdr.height + 3) >> 2;
        sub[4].width = (dec->ihdr.width + 1) >> 1;
        sub[4].height = (dec->ihdr.height + 1) >> 2;
        sub[5].width = dec->ihdr.width >> 1;
        sub[5].height = (dec->ihdr.height + 1) >> 1;
        sub[6].width = dec->ihdr.width;
        sub[6].height = dec->ihdr.height >> 1;
    }
    else
    {
        sub[0].width = dec->ihdr.width;
        sub[0].height = dec->ihdr.height;
    }


    if(flags & SPNG_DECODE_USE_GAMA && dec->have_gama)
    {
        float file_gamma = (float)dec->gama / 100000.0f;
        float max;

        uint32_t i, lut_entries;

        if(fmt == SPNG_FMT_RGBA8)
        {
            lut_entries = 256;
            max = 255.0f;
        }
        else /* SPNG_FMT_RGBA16 */
        {
            lut_entries = 65536;
            max = 65535.0f;
        }

        if(dec->lut_entries != lut_entries)
        {
            if(dec->gamma_lut != NULL) free(dec->gamma_lut);

            dec->gamma_lut = malloc(lut_entries * sizeof(uint16_t));
            if(dec->gamma_lut == NULL)
            {
                inflateEnd(&stream);
                free(scanline_orig);
                free(prev_scanline);
                return SPNG_EMEM;
            }
        }

        for(i=0; i < lut_entries; i++)
        {
            float screen_gamma = 2.2f;
            float exp = 1.0f / (file_gamma * screen_gamma);
            float c = pow((float)i / max, exp) * max;

            dec->gamma_lut[i] = c;
        }

        dec->lut_entries = lut_entries;
    }

    uint8_t red_sbits, green_sbits, blue_sbits, alpha_sbits, greyscale_sbits;

    red_sbits = dec->ihdr.bit_depth;
    green_sbits = dec->ihdr.bit_depth;
    blue_sbits = dec->ihdr.bit_depth;
    alpha_sbits = dec->ihdr.bit_depth;
    greyscale_sbits = dec->ihdr.bit_depth;

    if(dec->ihdr.colour_type == SPNG_COLOUR_TYPE_INDEXED)
    {
        red_sbits = 8;
        green_sbits = 8;
        blue_sbits = 8;
        alpha_sbits = 8;
    }

    if(dec->have_sbit)
    {
        if(flags & SPNG_DECODE_USE_SBIT)
        {
            if(dec->ihdr.colour_type == 0)
            {
                greyscale_sbits = dec->sbit.greyscale_bits;
                alpha_sbits = dec->ihdr.bit_depth;
            }
            if(dec->ihdr.colour_type == 2 || dec->ihdr.colour_type == 3)
            {
                red_sbits = dec->sbit.red_bits;
                green_sbits = dec->sbit.green_bits;
                blue_sbits = dec->sbit.blue_bits;
                alpha_sbits = dec->ihdr.bit_depth;
            }
            else if(dec->ihdr.colour_type == 4)
            {
                greyscale_sbits = dec->sbit.greyscale_bits;
                alpha_sbits = dec->sbit.alpha_bits;
            }
            else /* == 6 */
            {
                red_sbits = dec->sbit.red_bits;
                green_sbits = dec->sbit.green_bits;
                blue_sbits = dec->sbit.blue_bits;
                alpha_sbits = dec->sbit.alpha_bits;
            }
        }
    }

    uint8_t depth_target = 8; /* FMT_RGBA8 */
    if(fmt == SPNG_FMT_RGBA16) depth_target = 16;

    struct spng_chunk chunk, next;

    memcpy(&chunk, &dec->first_idat, sizeof(struct spng_chunk));

    uint32_t actual_crc;

    /* chunk data left for current IDAT, only used when streaming */
    uint32_t bytes_left = 0;

    if(dec->streaming)
    {
        bytes_left = chunk.length;
        uint32_t len = 8192;
        if(bytes_left < len) len = bytes_left;

        ret = dec->read_fn(dec, dec->read_user_ptr, dec->data, len);
        if(ret) goto decode_err;

        bytes_left -= len;
        actual_crc = crc32(0, NULL, 0);
        actual_crc = crc32(actual_crc, chunk.type, 4);
        actual_crc = crc32(actual_crc, dec->data, len);

        if(!bytes_left)
        {
            ret = dec->read_fn(dec, dec->read_user_ptr, &chunk.crc, 4);
            if(ret) goto decode_err;

            chunk.crc = ntohl(chunk.crc);

            if(actual_crc != chunk.crc)
            {
                ret = SPNG_ECHUNK_CRC;
                goto decode_err;
            }
        }

        stream.avail_in = len;
        stream.next_in = dec->data;
    }
    else
    {
        ret = get_chunk_data(dec, &chunk);
        if(ret) goto decode_err;

        stream.avail_in = chunk.length;
        stream.next_in = dec->data + chunk.offset + 8;
    }

    int pass;
    uint32_t scanline_idx;
    for(pass=0; pass < 7; pass++)
    {
        /* Skip empty passes or anything past [0] when interlaced==0 */
        if(sub[pass].width == 0 || sub[pass].height == 0) continue;

        /* Recalculate scanline_width for every subimage */
        /* Omitting overflow checks, we already did a worst-case calculation for *buf's size */
        scanline_width = sub[pass].width * channels * dec->ihdr.bit_depth + 8;
        if(scanline_width % 8 !=0) scanline_width = scanline_width + 8 - (scanline_width % 8);
        scanline_width /= 8;

        /* prev_scanline is all zeros for the first scanline */
        memset(prev_scanline, 0, scanline_width);

        /* Decompress one scanline at a time for each subimage */
        for(scanline_idx=0; scanline_idx < sub[pass].height; scanline_idx++)
        {
            stream.avail_out = scanline_width;
            stream.next_out = scanline;

            do
            {
                ret = inflate(&stream, Z_SYNC_FLUSH);

                if(ret != Z_OK)
                {
                    if(ret == Z_STREAM_END) /* zlib reached an end-marker */
                    {/* we don't have a full scanline or there are more scanlines left */
                        if(stream.avail_out!=0 || scanline_idx != (sub[pass].height - 1))
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

                /* We don't have scanline_width of data and we ran out of data for this chunk */
                if(stream.avail_out !=0 && stream.avail_in == 0)
                {
                    ret = next_header(dec, &chunk, &next);
                    if(ret) goto decode_err;

                    memcpy(&chunk, &next, sizeof(struct spng_chunk));

                    if(memcmp(chunk.type, type_idat, 4))
                    {
                        ret = SPNG_EIDAT_TOO_SHORT;
                        goto decode_err;
                    }

                    if(dec->streaming)
                    {
                        bytes_left = chunk.length;
                        uint32_t len = 8192;
                        if(bytes_left < len) len = bytes_left;

                        ret = dec->read_fn(dec, dec->read_user_ptr, dec->data, len);
                        if(ret) goto decode_err;

                        bytes_left -= len;

                        actual_crc = crc32(0, NULL, 0);
                        actual_crc = crc32(actual_crc, chunk.type, 4);
                        actual_crc = crc32(actual_crc, dec->data, len);

                        if(!bytes_left)
                        {
                            ret = dec->read_fn(dec, dec->read_user_ptr, &chunk.crc, 4);
                            if(ret) goto decode_err;

                            chunk.crc = ntohl(chunk.crc);

                            if(actual_crc != chunk.crc)
                            {
                                ret = SPNG_ECHUNK_CRC;
                                goto decode_err;
                            }
                        }

                        stream.avail_in = len;
                        stream.next_in = dec->data;
                    }
                    else
                    {
                        ret = get_chunk_data(dec, &chunk);
                        if(ret) goto decode_err;

                        stream.avail_in = chunk.length;
                        stream.next_in = dec->data + chunk.offset + 8;
                    }
                }

            }while(stream.avail_out != 0);

            ret = defilter_scanline(prev_scanline, scanline, scanline_width, bytes_per_pixel);
            if(ret) goto decode_err;

            uint32_t k;
            uint8_t r_8, g_8, b_8, a_8, gray_8;
            uint16_t r_16, g_16, b_16, a_16, gray_16;
            uint16_t r, g, b, a, gray;
            unsigned char pixel[8] = {0};

            r=0; g=0; b=0; a=0; gray=0;
            r_8=0; g_8=0; b_8=0; a_8=0; gray_8=0;
            r_16=0; g_16=0; b_16=0; a_16=0; gray_16=0;

            scanline++; /* increment past filter byte, keep indexing 0-based */

            /* Process a scanline per-pixel and write to *out */
            for(k=0; k < sub[pass].width; k++)
            {
                /* Extract a pixel from the scanline,
                   *_16/8 variables are used for memcpy'ing depending on bit_depth,
                   r, g, b, a, gray (all 16bits) are used for processing */
                switch(dec->ihdr.colour_type)
                {
                    case SPNG_COLOUR_TYPE_GRAYSCALE:
                    {
                        if(dec->ihdr.bit_depth == 16)
                        {
                            memcpy(&gray_16, scanline + (k * 2), 2);

                            gray_16 = ntohs(gray_16);

                            if(dec->have_trns &&
                               flags & SPNG_DECODE_USE_TRNS &&
                               dec->trns.type0_grey_sample == gray_16) a_16 = 0;
                            else a_16 = 65535;
                        }
                        else /* <= 8 */
                        {
                            memcpy(&gray_8, scanline + k / (8 / dec->ihdr.bit_depth), 1);

                            uint16_t mask16 = (1 << dec->ihdr.bit_depth) - 1;
                            uint8_t mask = mask16; /* avoid shift by width */
                            uint8_t samples_per_byte = 8 / dec->ihdr.bit_depth;
                            uint8_t max_shift_amount = 8 - dec->ihdr.bit_depth;
                            uint8_t shift_amount = max_shift_amount - ((k % samples_per_byte) * dec->ihdr.bit_depth);

                            gray_8 = gray_8 & (mask << shift_amount);
                            gray_8 = gray_8 >> shift_amount;

                            if(dec->have_trns &&
                               flags & SPNG_DECODE_USE_TRNS &&
                               dec->trns.type0_grey_sample == gray_8) a_8 = 0;
                            else a_8 = 255;
                        }

                        break;
                    }
                    case SPNG_COLOUR_TYPE_TRUECOLOR:
                    {
                        if(dec->ihdr.bit_depth == 16)
                        {
                            memcpy(&r_16, scanline + (k * 6), 2);
                            memcpy(&g_16, scanline + (k * 6) + 2, 2);
                            memcpy(&b_16, scanline + (k * 6) + 4, 2);

                            r_16 = ntohs(r_16);
                            g_16 = ntohs(g_16);
                            b_16 = ntohs(b_16);

                            if(dec->have_trns &&
                               flags & SPNG_DECODE_USE_TRNS &&
                               dec->trns.type2.red == r_16 &&
                               dec->trns.type2.green == g_16 &&
                               dec->trns.type2.blue == b_16) a_16 = 0;
                            else a_16 = 65535;
                        }
                        else /* == 8 */
                        {
                            memcpy(&r_8, scanline + (k * 3), 1);
                            memcpy(&g_8, scanline + (k * 3) + 1, 1);
                            memcpy(&b_8, scanline + (k * 3) + 2, 1);

                            if(dec->have_trns &&
                               flags & SPNG_DECODE_USE_TRNS &&
                               dec->trns.type2.red == r_8 &&
                               dec->trns.type2.green == g_8 &&
                               dec->trns.type2.blue == b_8) a_8 = 0;
                            else a_8 = 255;
                        }

                        break;
                    }
                    case SPNG_COLOUR_TYPE_INDEXED:
                    {
                        uint8_t entry = 0;
                        memcpy(&entry, scanline + k / (8 / dec->ihdr.bit_depth), 1);

                        uint16_t mask16 = (1 << dec->ihdr.bit_depth) - 1;
                        uint8_t mask = mask16; /* avoid shift by width */
                        uint8_t samples_per_byte = 8 / dec->ihdr.bit_depth;
                        uint8_t max_shift_amount = 8 - dec->ihdr.bit_depth;
                        uint8_t shift_amount = max_shift_amount - ((k % samples_per_byte) * dec->ihdr.bit_depth);

                        entry = entry & (mask << shift_amount);
                        entry = entry >> shift_amount;

                        if(entry < dec->plte.n_entries)
                        {
                            r_8 = dec->plte.entries[entry].red;
                            g_8 = dec->plte.entries[entry].green;
                            b_8 = dec->plte.entries[entry].blue;
                        }
                        else
                        {
                            ret = SPNG_EPLTE_IDX;
                            goto decode_err;
                        }

                        if(dec->have_trns &&
                           flags & SPNG_DECODE_USE_TRNS &&
                           (entry < dec->trns.n_type3_entries)) a_8 = dec->trns.type3_alpha[entry];
                        else a_8 = 255;

                        break;
                    }
                    case SPNG_COLOUR_TYPE_GRAYSCALE_ALPHA:
                    {
                        if(dec->ihdr.bit_depth == 16)
                        {
                            memcpy(&gray_16, scanline + (k * 4), 2);
                            memcpy(&a_16, scanline + (k * 4) + 2, 2);

                            gray_16 = ntohs(gray_16);
                            a_16 = ntohs(a_16);

                        }
                        else /* == 8 */
                        {
                            memcpy(&gray_8, scanline + (k * 2), 1);
                            memcpy(&a_8, scanline + (k * 2) + 1, 1);
                        }

                        break;
                    }
                    case SPNG_COLOUR_TYPE_TRUECOLOR_ALPHA:
                    {
                        if(dec->ihdr.bit_depth == 16)
                        {
                            memcpy(&r_16, scanline + (k * 8), 2);
                            memcpy(&g_16, scanline + (k * 8) + 2, 2);
                            memcpy(&b_16, scanline + (k * 8) + 4, 2);
                            memcpy(&a_16, scanline + (k * 8) + 6, 2);

                            r_16 = ntohs(r_16);
                            g_16 = ntohs(g_16);
                            b_16 = ntohs(b_16);
                            a_16 = ntohs(a_16);
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
                }/* switch(dec->ihdr.colour_type) */


                if(dec->ihdr.bit_depth == 16)
                {
                    r = r_16; g = g_16; b = b_16; a = a_16;
                    gray = gray_16;
                }
                else
                {
                    r = r_8; g = g_8; b = b_8; a = a_8;
                    gray = gray_8;
                }


                if(dec->ihdr.colour_type == SPNG_COLOUR_TYPE_GRAYSCALE ||
                   dec->ihdr.colour_type == SPNG_COLOUR_TYPE_GRAYSCALE_ALPHA)
                {
                    gray = sample_to_target(gray, dec->ihdr.bit_depth, greyscale_sbits, depth_target);
                    a = sample_to_target(a, dec->ihdr.bit_depth, alpha_sbits, depth_target);
                }
                else
                {
                    uint8_t processing_depth = dec->ihdr.bit_depth;
                    if(dec->ihdr.colour_type == SPNG_COLOUR_TYPE_INDEXED) processing_depth = 8;

                    r = sample_to_target(r, processing_depth, red_sbits, depth_target);
                    g = sample_to_target(g, processing_depth, green_sbits, depth_target);
                    b = sample_to_target(b, processing_depth, blue_sbits, depth_target);
                    a = sample_to_target(a, processing_depth, alpha_sbits, depth_target);
                }



                if(dec->ihdr.colour_type == SPNG_COLOUR_TYPE_GRAYSCALE ||
                   dec->ihdr.colour_type == SPNG_COLOUR_TYPE_GRAYSCALE_ALPHA)
                {
                    r = gray;
                    g = gray;
                    b = gray;
                }


                if(flags & SPNG_DECODE_USE_GAMA && dec->have_gama)
                {
                    r = dec->gamma_lut[r];
                    g = dec->gamma_lut[g];
                    b = dec->gamma_lut[b];
                }


                size_t pixel_size;
                size_t pixel_offset;

                /* only use *_8/16 for memcpy */
                r_8 = r; g_8 = g; b_8 = b; a_8 = a;
                r_16 = r; g_16 = g; b_16 = b; a_16 = a;
                gray_8 = gray;
                gray_16 = gray;

                if(fmt == SPNG_FMT_RGBA8)
                {
                    pixel_size = 4;

                    memcpy(pixel, &r_8, 1);
                    memcpy(pixel + 1, &g_8, 1);
                    memcpy(pixel + 2, &b_8, 1);
                    memcpy(pixel + 3, &a_8, 1);
                }
                else if(fmt == SPNG_FMT_RGBA16)
                {
                    pixel_size = 8;

                    memcpy(pixel, &r_16, 2);
                    memcpy(pixel + 2, &g_16, 2);
                    memcpy(pixel + 4, &b_16, 2);
                    memcpy(pixel + 6, &a_16, 2);
                }


                if(!dec->ihdr.interlace_method)
                {
                    pixel_offset = (k + (scanline_idx * dec->ihdr.width)) * pixel_size;
                }
                else
                {
                    const unsigned int adam7_x_start[7] = { 0, 4, 0, 2, 0, 1, 0 };
                    const unsigned int adam7_y_start[7] = { 0, 0, 4, 0, 2, 0, 1 };
                    const unsigned int adam7_x_delta[7] = { 8, 8, 4, 4, 2, 2, 1 };
                    const unsigned int adam7_y_delta[7] = { 8, 8, 8, 4, 4, 2, 2 };

                    pixel_offset = ((adam7_y_start[pass] + scanline_idx * adam7_y_delta[pass]) *
                                     dec->ihdr.width + adam7_x_start[pass] + k * adam7_x_delta[pass]) * pixel_size;
                }

                memcpy(out + pixel_offset, pixel, pixel_size);

            }/* for(k=0; k < sub[pass].width; k++) */

            scanline--; /* point to filter byte */

            /* NOTE: prev_scanline is always defiltered */
            memcpy(prev_scanline, scanline, scanline_width);

        }/* for(scanline_idx=0; scanline_idx < sub[pass].height; scanline_idx++) */
    }/* for(pass=0; pass < 7; pass++) */

    /* zlib stream ended before an IDAT chunk boundary */
    if(dec->streaming && bytes_left)
    {/* read the rest of the chunk and check crc */
        while(bytes_left)
        {
            uint32_t len = 8192;
            if(bytes_left < len) len = bytes_left;

            ret = dec->read_fn(dec, dec->read_user_ptr, dec->data, len);
            if(ret) goto decode_err;

            bytes_left -= len;
        }

        ret = dec->read_fn(dec, dec->read_user_ptr, &chunk.crc, 4);
        if(ret) goto decode_err;

        chunk.crc = ntohl(chunk.crc);

        if(actual_crc != chunk.crc)
        {
            ret = SPNG_ECHUNK_CRC;
            goto decode_err;
        }
    }

decode_err:

    inflateEnd(&stream);
    free(scanline_orig);
    free(prev_scanline);

    if(ret)
    {
        dec->valid_state = 0;
        return ret;
    }

    memcpy(&dec->last_idat, &chunk, sizeof(struct spng_chunk));

    ret = validate_past_idat(dec);

    if(ret) dec->valid_state = 0;

    return ret;
}

