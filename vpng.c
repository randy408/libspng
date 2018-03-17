#include "vpng.h"

#include <arpa/inet.h>
#include <limits.h>
#include <string.h>

#include <zlib.h>

#define VPNG_FILTER_TYPE_NONE 0
#define VPNG_FILTER_TYPE_SUB 1
#define VPNG_FILTER_TYPE_UP 2
#define VPNG_FILTER_TYPE_AVERAGE 3
#define VPNG_FILTER_TYPE_PAETH 4

struct vpng_subimage
{
    uint32_t width;
    uint32_t height;
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
    if(required < bytes) return VPNG_EOVERFLOW;

    if(required > data_size) return VPNG_EOF; /* buffer too small */

    return 0;
}

/* Gets and validates the next chunk's offset, length, type and crc.
   On success the next chunk's data is safe to read from dec->data,
   if header_only is non-zero then only the next chunk's header is checked and
   chunk data should not be accessed.
   On error: EINVAL
             EOVERFLOW
             EOF
             ECHUNK_SIZE
             ECHUNK_CRC
 */
static int next_chunk(struct vpng_decoder *dec, const struct vpng_chunk *current,
                      struct vpng_chunk *next, int header_only)
{
    if(dec == NULL || current == NULL || next == NULL) return 1;
/*  Call require_bytes() twice: once with all of current chunk's bytes + next header,
    then all of next chunk's bytes if the header is valid and header_only is 0 */
    size_t bytes_required;

    bytes_required = current->length;
    bytes_required += 20; /* current header, current crc, next header */
    if(bytes_required < current->length) return VPNG_EOVERFLOW;

    int ret = require_bytes(current->offset, bytes_required, dec->data_size);
    if(ret) return ret;

    /* require_bytes() already did an overflow check */
    next->offset = current->offset + bytes_required - 8;

    memcpy(&next->length, dec->data + next->offset, 4);

    next->length = ntohl(next->length);

    if(next->length > png_u32max)
    {
        memset(next, 0, sizeof(struct vpng_chunk));
        return VPNG_ECHUNK_SIZE;
    }

    memcpy(&next->type, dec->data + next->offset + 4, 4);

    bytes_required = next->length;
    bytes_required += 12; /* next header, next crc */

    if(bytes_required < next->length) return VPNG_EOVERFLOW;

    ret = require_bytes(next->offset, bytes_required, dec->data_size);
    if(ret)
    {
        memset(next, 0, sizeof(struct vpng_chunk));
        return ret;
    }

    memcpy(&next->crc, dec->data + next->offset + 8 + next->length, 4);
    next->crc = ntohl(next->crc);

    uint32_t actual_crc = crc32(0, Z_NULL, 0);
    actual_crc = crc32(actual_crc, dec->data + next->offset + 4, next->length + 4);

    if(actual_crc != next->crc)
    {
        memset(next, 0, sizeof(struct vpng_chunk));
        return VPNG_ECHUNK_CRC;
    }

    return 0;
}

static int check_ihdr(struct vpng_ihdr *ihdr)
{
    if(ihdr->width > png_u32max) return VPNG_EWIDTH;
    if(ihdr->height > png_u32max) return VPNG_EHEIGHT;

    switch(ihdr->colour_type)
    {
        case VPNG_COLOUR_TYPE_GRAYSCALE:
        {
            if( !(ihdr->bit_depth == 1 || ihdr->bit_depth == 2 ||
                  ihdr->bit_depth == 4 || ihdr->bit_depth == 8 ||
                  ihdr->bit_depth == 16) )
                  return VPNG_EBIT_DEPTH;

            break;
        }
        case VPNG_COLOUR_TYPE_TRUECOLOR:
        {
            if( !(ihdr->bit_depth == 8 || ihdr->bit_depth == 16) )
                return VPNG_EBIT_DEPTH;

            break;
        }
        case VPNG_COLOUR_TYPE_INDEXED_COLOUR:
        {
            if( !(ihdr->bit_depth == 1 || ihdr->bit_depth == 2 ||
                  ihdr->bit_depth == 4 || ihdr->bit_depth == 8) )
                return VPNG_EBIT_DEPTH;

            break;
        }
        case VPNG_COLOUR_TYPE_GRAYSCALE_WITH_ALPHA:
        {
            if( !(ihdr->bit_depth == 8 || ihdr->bit_depth == 16) )
                return VPNG_EBIT_DEPTH;

            break;
        }
        case VPNG_COLOUR_TYPE_TRUECOLOR_WITH_ALPHA:
        {
            if( !(ihdr->bit_depth == 8 || ihdr->bit_depth == 16) )
                return VPNG_EBIT_DEPTH;

            break;
        }
    default: return VPNG_ECOLOUR_TYPE;
    }

    if(ihdr->compression_method || ihdr->filter_method)
        return VPNG_ECOMPRESSION_METHOD;

    if( !(ihdr->interlace_method == 0 || ihdr->interlace_method == 1) )
        return VPNG_EINTERLACE_METHOD;

    return 0;
}

static int check_sig_get_ihdr(struct vpng_decoder *dec)
{
    if(dec==NULL) return 1;

    uint8_t signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if(memcmp(dec->data, signature, sizeof(signature))) return VPNG_ESIGNATURE;

    struct vpng_chunk chunk;

    size_t sizeof_signature_and_ihdr = 33;
    if(dec->data_size < sizeof_signature_and_ihdr) return VPNG_EOF;

    memcpy(&chunk.length, dec->data + 8, 4);
    memcpy(&chunk.type, dec->data + 12, 4);
    memcpy(&chunk.crc, dec->data + 29, 4);

    chunk.length = ntohl(chunk.length);
    chunk.crc = ntohl(chunk.crc);

    uint32_t actual_crc = crc32(0, Z_NULL, 0);
    actual_crc = crc32(actual_crc, dec->data + 12, 17);
    if(actual_crc != chunk.crc) return VPNG_ECHUNK_CRC;

    if(chunk.length != 13) return VPNG_EIHDR_SIZE;
    if(memcmp(chunk.type, type_ihdr, 4)) return VPNG_ENOIHDR;

    struct vpng_ihdr ihdr;

    memcpy(&ihdr.width, dec->data + 16, 4);
    memcpy(&ihdr.height, dec->data + 20, 4);
    memcpy(&ihdr.bit_depth, dec->data + 24, 1);
    memcpy(&ihdr.colour_type, dec->data + 25, 1);
    memcpy(&ihdr.compression_method, dec->data + 26, 1);
    memcpy(&ihdr.filter_method, dec->data + 27, 1);
    memcpy(&ihdr.interlace_method, dec->data + 28, 1);

    ihdr.width = ntohl(ihdr.width);
    ihdr.height = ntohl(ihdr.height);

    int ret = check_ihdr(&ihdr);
    if(ret) return ret;

    memcpy(&dec->ihdr, &ihdr, sizeof(struct vpng_ihdr));

    dec->have_ihdr = 1;

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

    if(filter > 4) return VPNG_EFILTER;
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
            case VPNG_FILTER_TYPE_SUB:
            {
                x = x + a;
                break;
            }
            case VPNG_FILTER_TYPE_UP:
            {
                x = x + b;
                break;
            }
            case VPNG_FILTER_TYPE_AVERAGE:
            {
                uint16_t avg = (a + b) / 2;
                x = x + avg;
                break;
            }
            case VPNG_FILTER_TYPE_PAETH:
            {
                x = x + paeth(a,b,c);
                break;
            }
        }

        memcpy(scanline + i, &x, 1);
    }

    return 0;
}

/*
    Read and validate all critical and relevant ancillary chunks up to the first IDAT
    Returns zero and sets dec->first_idat
*/
static int get_ancillary_data_first_idat(struct vpng_decoder *dec)
{
    if(dec==NULL) return 1;
    if(!dec->valid_state) return VPNG_EBADSTATE;

    struct vpng_chunk chunk, next;
    /* use IHDR as current chunk */
    chunk.offset = 8;
    chunk.length = 13;

    unsigned char *data;

    while(!next_chunk(dec, &chunk, &next, 1)) /* read header */
    {
        memcpy(&chunk, &next, sizeof(struct vpng_chunk));

        if(!memcmp(chunk.type, type_idat, 4))
        {
            memcpy(&dec->first_idat, &chunk, sizeof(struct vpng_chunk));
            return 0;
        }

        int ret = next_chunk(dec, &chunk, &next, 0); /* read data, check crc */
        if(ret) return ret;

        data = dec->data + chunk.offset + 8;

        /* Reserved bit must be zero */
        if( (chunk.type[2] & (1 << 5)) != 0) return VPNG_ECHUNK_TYPE;
        /* Ignore private chunks */
        if( (chunk.type[1] & (1 << 5)) != 0) continue;

        if( (chunk.type[0] & (1 << 5)) == 0) /* Critical chunk */
        {
            if(!memcmp(chunk.type, type_plte, 4))
            {
                if(chunk.length % 3 !=0) return VPNG_ECHUNK_SIZE;
                if( (chunk.length / 3) > 256 ) return VPNG_ECHUNK_SIZE;
                if(dec->n_plte_entries > ( (1 << dec->ihdr.bit_depth) -1 ) ) return VPNG_ECHUNK_SIZE;

                dec->n_plte_entries = chunk.length / 3;

                size_t i;
                for(i=0; i < dec->n_plte_entries; i++)
                {
                    memcpy(&dec->plte_entries[i].red,   data + i * 3, 1);
                    memcpy(&dec->plte_entries[i].green, data + i * 3 + 1, 1);
                    memcpy(&dec->plte_entries[i].blue,  data + i * 3 + 2, 1);
                }

                dec->plte_offset = chunk.offset;

                dec->have_plte = 1;
            }
            else if(!memcmp(chunk.type, type_iend, 4)) return VPNG_ECHUNK_POS;
            else if(!memcmp(chunk.type, type_ihdr, 4)) return VPNG_ECHUNK_POS;
            else return VPNG_ECHUNK_UNKNOWN_CRITICAL;
        }
        else if(!memcmp(chunk.type, type_chrm, 4)) /* Ancillary chunks */
        {
            if(dec->have_plte && chunk.offset > dec->plte_offset) return VPNG_ECHUNK_POS;
            if(dec->have_chrm) return VPNG_EDUP_CHRM;

            if(chunk.length != 32) return VPNG_ECHUNK_SIZE;

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

            dec->have_chrm = 1;
        }
        else if(!memcmp(chunk.type, type_gama, 4))
        {
            if(dec->have_plte && chunk.offset > dec->plte_offset) return VPNG_ECHUNK_POS;
            if(dec->have_gama) return VPNG_EDUP_GAMA;

            if(chunk.length != 4) return VPNG_ECHUNK_SIZE;

            memcpy(&dec->gama, data, 4);

            dec->gama = ntohl(dec->gama);

            dec->have_gama = 1;
        }
        else if(!memcmp(chunk.type, type_iccp, 4))
        {
            if(dec->have_plte && chunk.offset > dec->plte_offset) return VPNG_ECHUNK_POS;
            if(dec->have_iccp) return VPNG_EDUP_ICCP;
            dec->have_iccp = 1;
            /* TODO: read */
        }
        else if(!memcmp(chunk.type, type_sbit, 4))
        {
            if(dec->have_plte && chunk.offset > dec->plte_offset) return VPNG_ECHUNK_POS;
            if(dec->have_sbit) return VPNG_EDUP_SBIT;

            if(dec->ihdr.colour_type == 0)
            {
                if(chunk.length != 1) return VPNG_ECHUNK_SIZE;

                memcpy(&dec->sbit_type0_greyscale_bits, data, 1);

                if(dec->sbit_type0_greyscale_bits == 0) return VPNG_ESBIT;
                if(dec->sbit_type0_greyscale_bits > dec->ihdr.bit_depth) return VPNG_ESBIT;
            }
            else if(dec->ihdr.colour_type == 2 || dec->ihdr.colour_type == 3)
            {
                if(chunk.length != 3) return VPNG_ECHUNK_SIZE;

                memcpy(&dec->sbit_type2_3.red_bits, data, 1);
                memcpy(&dec->sbit_type2_3.green_bits, data + 1 , 1);
                memcpy(&dec->sbit_type2_3.blue_bits, data + 2, 1);

                if(dec->sbit_type2_3.red_bits == 0) return VPNG_ESBIT;
                if(dec->sbit_type2_3.green_bits == 0) return VPNG_ESBIT;
                if(dec->sbit_type2_3.blue_bits == 0) return VPNG_ESBIT;

                uint8_t bit_depth;
                if(dec->ihdr.colour_type == 3) bit_depth = 8;
                else bit_depth = dec->ihdr.bit_depth;

                if(dec->sbit_type2_3.red_bits > bit_depth) return VPNG_ESBIT;
                if(dec->sbit_type2_3.green_bits > bit_depth) return VPNG_ESBIT;
                if(dec->sbit_type2_3.blue_bits > bit_depth) return VPNG_ESBIT;
            }
            else if(dec->ihdr.colour_type == 4)
            {
                if(chunk.length != 2) return VPNG_ECHUNK_SIZE;

                memcpy(&dec->sbit_type4.greyscale_bits, data, 1);
                memcpy(&dec->sbit_type4.alpha_bits, data + 1, 1);

                if(dec->sbit_type4.greyscale_bits == 0) return VPNG_ESBIT;
                if(dec->sbit_type4.greyscale_bits > dec->ihdr.bit_depth) return VPNG_ESBIT;
            }
            else if(dec->ihdr.colour_type == 6)
            {
                if(chunk.length != 4) return VPNG_ECHUNK_SIZE;

                memcpy(&dec->sbit_type6.red_bits, data, 1);
                memcpy(&dec->sbit_type6.green_bits, data + 1, 1);
                memcpy(&dec->sbit_type6.blue_bits, data + 2, 1);
                memcpy(&dec->sbit_type6.alpha_bits, data + 3, 1);

                if(dec->sbit_type6.red_bits == 0) return VPNG_ESBIT;
                if(dec->sbit_type6.green_bits == 0) return VPNG_ESBIT;
                if(dec->sbit_type6.blue_bits == 0) return VPNG_ESBIT;
                if(dec->sbit_type6.alpha_bits == 0) return VPNG_ESBIT;

                if(dec->sbit_type6.red_bits > dec->ihdr.bit_depth) return VPNG_ESBIT;
                if(dec->sbit_type6.green_bits > dec->ihdr.bit_depth) return VPNG_ESBIT;
                if(dec->sbit_type6.blue_bits > dec->ihdr.bit_depth) return VPNG_ESBIT;
                if(dec->sbit_type6.alpha_bits > dec->ihdr.bit_depth) return VPNG_ESBIT;
            }

            dec->have_sbit = 1;
        }
        else if(!memcmp(chunk.type, type_srgb, 4))
        {
            if(dec->have_plte && chunk.offset > dec->plte_offset) return VPNG_ECHUNK_POS;
            if(dec->have_srgb) return VPNG_EDUP_SRGB;

            if(chunk.length != 1) return VPNG_ECHUNK_SIZE;

            memcpy(&dec->srgb_rendering_intent, data, 1);

            dec->have_srgb = 1;
        }
        else if(!memcmp(chunk.type, type_bkgd, 4))
        {
            if(dec->have_plte && chunk.offset < dec->plte_offset) return VPNG_ECHUNK_POS;
            if(dec->have_bkgd) return VPNG_EDUP_BKGD;

            if(dec->ihdr.colour_type == 0 || dec->ihdr.colour_type == 4)
            {
                if(chunk.length != 2) return VPNG_ECHUNK_SIZE;

                memcpy(&dec->bkgd_type0_4_greyscale, data, 2);

                uint16_t mask = ~0;
                if(dec->ihdr.bit_depth < 16) mask = (1 << dec->ihdr.bit_depth) - 1;

                dec->bkgd_type0_4_greyscale = ntohs(dec->bkgd_type0_4_greyscale) & mask;
            }
            else if(dec->ihdr.colour_type == 2 || dec->ihdr.colour_type == 6)
            {
                if(chunk.length != 6) return VPNG_ECHUNK_SIZE;

                memcpy(&dec->bkgd_type2_6.red, data, 2);
                memcpy(&dec->bkgd_type2_6.green, data + 2, 2);
                memcpy(&dec->bkgd_type2_6.blue, data + 4, 2);

                uint16_t mask = ~0;
                if(dec->ihdr.bit_depth < 16) mask = (1 << dec->ihdr.bit_depth) - 1;

                dec->bkgd_type2_6.red = ntohs(dec->bkgd_type2_6.red) & mask;
                dec->bkgd_type2_6.green = ntohs(dec->bkgd_type2_6.green) & mask;
                dec->bkgd_type2_6.blue = ntohs(dec->bkgd_type2_6.blue) & mask;
            }
            else if(dec->ihdr.colour_type == 3)
            {
                if(chunk.length != 1) return VPNG_ECHUNK_SIZE;
                if(!dec->have_plte) return VPNG_EBKGD_NO_PLTE;

                memcpy(&dec->bkgd_type3_plte_index, data, 1);
                if(dec->bkgd_type3_plte_index >= dec->n_plte_entries) return VPNG_EBKGD_PLTE_IDX;
            }

            dec->have_bkgd = 1;
        }
        else if(!memcmp(chunk.type, type_trns, 4))
        {
            if(dec->have_plte && chunk.offset < dec->plte_offset) return VPNG_ECHUNK_POS;
            if(dec->have_trns) return VPNG_EDUP_TRNS;
            if(!chunk.length) return VPNG_ECHUNK_SIZE;
            /* XXX: spec isn't explicit about forbidding 0 entries */

            if(dec->ihdr.colour_type == 0)
            {
                if(chunk.length != 2) return VPNG_ECHUNK_SIZE;

                memcpy(&dec->trns_type0_grey_sample, data, 2);

                dec->trns_type0_grey_sample = ntohs(dec->trns_type0_grey_sample);
            }
            else if(dec->ihdr.colour_type == 2)
            {
                if(chunk.length != 6) return VPNG_ECHUNK_SIZE;

                memcpy(&dec->trns_type2.red, data, 2);
                memcpy(&dec->trns_type2.green, data + 2, 2);
                memcpy(&dec->trns_type2.blue, data + 4, 2);

                dec->trns_type2.red = ntohs(dec->trns_type2.red);
                dec->trns_type2.green = ntohs(dec->trns_type2.green);
                dec->trns_type2.blue = ntohs(dec->trns_type2.blue);
            }
            else if(dec->ihdr.colour_type == 3)
            {
                if(chunk.length > dec->n_plte_entries) return VPNG_ECHUNK_SIZE;

                size_t k;
                for(k=0; k < chunk.length; k++)
                {
                    memcpy(&dec->trns_type3_alpha[k], data + k, 1);
                }
                dec->n_trns_type3_entries = chunk.length;
            }
            else return VPNG_ETRNS_COLOUR_TYPE;

            dec->have_trns = 1;
        }
        else if(!memcmp(chunk.type, type_hist, 4))
        {
            if(!dec->have_plte) return 1;
            if(chunk.offset < dec->plte_offset) return VPNG_ECHUNK_POS;
            if(dec->have_hist) return VPNG_EDUP_HIST;

            if( (chunk.length / 2) != (dec->n_plte_entries) ) return VPNG_ECHUNK_SIZE;

            size_t k;
            for(k=0; k < (chunk.length / 2); k++)
            {
                memcpy(&dec->hist_frequency[k], data + k*2, 2);

                dec->hist_frequency[k] = ntohs(dec->hist_frequency[k]);
            }

            dec->have_hist = 1;
        }
        else if(!memcmp(chunk.type, type_phys, 4))
        {
            if(dec->have_phys) return VPNG_EDUP_PHYS;

            if(chunk.length != 9) return VPNG_ECHUNK_SIZE;

            memcpy(&dec->phys_ppu_x, data, 4);
            memcpy(&dec->phys_ppu_y, data + 4, 4);
            memcpy(&dec->phys_unit_specifier, data + 8, 4);

            dec->phys_ppu_x = ntohl(dec->phys_ppu_x);
            dec->phys_ppu_y = ntohl(dec->phys_ppu_y);
            dec->phys_unit_specifier = ntohl(dec->phys_unit_specifier);

            dec->have_phys = 1;
        }
        else if(!memcmp(chunk.type, type_splt, 4))
        {/* TODO: read */
        }
        else if(!memcmp(chunk.type, type_time, 4))
        {
            if(dec->have_time) return VPNG_EDUP_TIME;

            if(chunk.length != 7) return VPNG_ECHUNK_SIZE;

            memcpy(&dec->time.year, data, 2);
            memcpy(&dec->time.month, data + 2, 1);
            memcpy(&dec->time.day, data + 3, 1);
            memcpy(&dec->time.hour, data + 4, 1);
            memcpy(&dec->time.minute, data + 5, 1);
            memcpy(&dec->time.second, data + 6, 1);

            dec->time.year = ntohs(dec->time.year);

            if(dec->time.month == 0 || dec->time.month > 12) return 1;
            if(dec->time.day == 0 || dec->time.day > 31) return 1;
            if(dec->time.hour > 23) return 1;
            if(dec->time.minute > 59) return 1;
            if(dec->time.second > 60) return 1;

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

    /* didn't reach an IDAT chunk */
    return next_chunk(dec, &chunk, &next, 1);
}

static int validate_past_idat(struct vpng_decoder *dec)
{
    if(dec == NULL) return 1;

    struct vpng_chunk chunk, next;

    memcpy(&chunk, &dec->last_idat, sizeof(struct vpng_chunk));

    while(!next_chunk(dec, &chunk, &next, 0))
    {
        memcpy(&chunk, &next, sizeof(struct vpng_chunk));

        /* Reserved bit must be zero */
        if( (chunk.type[2] & (1 << 5)) != 0) return VPNG_ECHUNK_TYPE;
         /* Ignore private chunks */
        if( (chunk.type[1] & (1 << 5)) != 0) continue;

        if(!memcmp(chunk.type, type_chrm, 4)) return VPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_gama, 4)) return VPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_iccp, 4)) return VPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_sbit, 4)) return VPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_srgb, 4)) return VPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_bkgd, 4)) return VPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_hist, 4)) return VPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_trns, 4)) return VPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_phys, 4)) return VPNG_ECHUNK_POS;
        if(!memcmp(chunk.type, type_splt, 4)) return VPNG_ECHUNK_POS;
    }

    int ret = next_chunk(dec, &chunk, &next, 0);

    if(ret != VPNG_EOF) return ret;

    /* last chunk must be IEND */
    if(memcmp(chunk.type, type_iend, 4)) return VPNG_ECHUNK_POS;

    /* IEND chunk must be at the very end */
    if(chunk.offset != (dec->data_size - 12) ) return VPNG_EDATA_AFTER_IEND;

    return 0;
}


/* Scale "sbits" significant bits in "sample" from "bit_depth" to "target"

   "bit_depth" must be a valid PNG depth
   "sbits" must be less than or equal to "bit_depth"
   "target" must be between 1 and 16
*/
static uint16_t sample_to_target(uint16_t sample, uint8_t bit_depth, uint8_t sbits, uint8_t target)
{/* XXX: libpng returns the sample unchanged when sbit is lower than target, instead of rightshifting+upsampling */
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


struct vpng_decoder * vpng_decoder_new(void)
{
    struct vpng_decoder *dec = malloc(sizeof(struct vpng_decoder));
    if(dec==NULL) return NULL;

    memset(dec, 0, sizeof(struct vpng_decoder));
    dec->valid_state = 1;

    return dec;
}

void vpng_decoder_free(struct vpng_decoder *dec)
{
    if(dec==NULL) return;

    memset(dec, 0, sizeof(struct vpng_decoder));

    free(dec);
}

int vpng_decoder_set_buffer(struct vpng_decoder *dec, void *buf, size_t size)
{
    if(dec==NULL || buf==NULL) return 1;
    if(!dec->valid_state) return VPNG_EBADSTATE;

    if(dec->data != NULL) return VPNG_EBUF_SET;

    dec->data = buf;
    dec->data_size = size;

    int ret = check_sig_get_ihdr(dec);
    if(ret) dec->valid_state = 0;

    return ret;
}


int vpng_get_ihdr(struct vpng_decoder *dec, struct vpng_ihdr *ihdr)
{
    if(dec==NULL || ihdr==NULL) return 1;
    if(dec->data == NULL) return 1;
    if(!dec->valid_state) return VPNG_EBADSTATE;

    memcpy(ihdr, &dec->ihdr, sizeof(struct vpng_ihdr));

    return 0;
}

int vpng_get_output_image_size(struct vpng_decoder *dec, int fmt, size_t *out)
{
    if(dec==NULL || out==NULL) return 1;

    if(!dec->valid_state) return VPNG_EBADSTATE;

    if(fmt == VPNG_FMT_RGBA8)
    {
        size_t res;

        if(4 > SIZE_MAX / dec->ihdr.width) return VPNG_EOVERFLOW;
        res = 4 * dec->ihdr.width;

        if(res > SIZE_MAX / dec->ihdr.height) return VPNG_EOVERFLOW;
        res = res * dec->ihdr.height;

        *out = res;
    }
    else return 1;

    return 0;
}

int vpng_get_image_rgba8(struct vpng_decoder *dec, void *out, size_t out_size)
{
    if(dec==NULL) return 1;
    if(out==NULL) return 1;
    if(!dec->valid_state) return VPNG_EBADSTATE;

    int ret;
    size_t out_size_required;

    ret = vpng_get_output_image_size(dec, VPNG_FMT_RGBA8, &out_size_required);
    if(ret) return ret;
    if(out_size < out_size_required) return VPNG_EBUFSIZ;

    if(!dec->first_idat.offset)
    {
        ret = get_ancillary_data_first_idat(dec);
        if(ret)
        {
            dec->valid_state = 0;
            return ret;
        }
    }

    uint8_t channels = 1; /* grayscale or indexed_colour */

    if(dec->ihdr.colour_type == VPNG_COLOUR_TYPE_TRUECOLOR) channels = 3;
    else if(dec->ihdr.colour_type == VPNG_COLOUR_TYPE_GRAYSCALE_WITH_ALPHA) channels = 2;
    else if(dec->ihdr.colour_type == VPNG_COLOUR_TYPE_TRUECOLOR_WITH_ALPHA) channels = 4;

    uint8_t bytes_per_pixel;

    if(dec->ihdr.bit_depth < 8) bytes_per_pixel = 1;
    else bytes_per_pixel = channels * (dec->ihdr.bit_depth / 8);

    /* Calculate scanline width in bits, round up to a multiple of 8, convert to bytes */
    size_t scanline_width = dec->ihdr.width;

    if(scanline_width > SIZE_MAX / channels) return VPNG_EOVERFLOW;
    scanline_width = scanline_width * channels;

    if(scanline_width > SIZE_MAX / dec->ihdr.bit_depth) return VPNG_EOVERFLOW;
    scanline_width = scanline_width * dec->ihdr.bit_depth;

    scanline_width = scanline_width + 8; /* filter byte */
    if(scanline_width < 8) return VPNG_EOVERFLOW;

    if(scanline_width % 8 != 0) /* round to up multiple of 8 */
    {
        scanline_width = scanline_width + 8;
        if(scanline_width < 8) return VPNG_EOVERFLOW;
        scanline_width = scanline_width - (scanline_width % 8);
    }

    scanline_width = scanline_width / 8;


    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    if(inflateInit(&stream) != Z_OK) return VPNG_EZLIB;

    unsigned char *scanline_orig, *scanline, *prev_scanline;

    scanline_orig = malloc(scanline_width);
    if(scanline_orig==NULL)
    {
        inflateEnd(&stream);
        return VPNG_EMEM;
    }

    /* Some of the error-handling goto's might leave scanline incremented,
       leading to a failed free(), this prevents that. */
    scanline = scanline_orig;

    prev_scanline = malloc(scanline_width);
    if(prev_scanline==NULL)
    {
        inflateEnd(&stream);
        free(scanline_orig);
        return VPNG_EMEM;
    }

    struct vpng_subimage sub[7];
    memset(sub, 0, sizeof(struct vpng_subimage) * 7);

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

    if(!dec->have_sbit)
    {
        dec->sbit_type0_greyscale_bits = dec->ihdr.bit_depth;

        dec->sbit_type2_3.red_bits = dec->ihdr.bit_depth;
        dec->sbit_type2_3.green_bits = dec->ihdr.bit_depth;
        dec->sbit_type2_3.blue_bits = dec->ihdr.bit_depth;

        dec->sbit_type4.greyscale_bits = dec->ihdr.bit_depth;
        dec->sbit_type4.alpha_bits = dec->ihdr.bit_depth;

        dec->sbit_type6.red_bits = dec->ihdr.bit_depth;
        dec->sbit_type6.green_bits = dec->ihdr.bit_depth;
        dec->sbit_type6.blue_bits = dec->ihdr.bit_depth;
        dec->sbit_type6.alpha_bits = dec->ihdr.bit_depth;
    }

    struct vpng_chunk chunk, next;

    memcpy(&chunk, &dec->first_idat, sizeof(struct vpng_chunk));

    stream.avail_in = chunk.length;
    stream.next_in = dec->data + chunk.offset + 8;

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
                            ret = VPNG_EIDAT_TOO_SHORT;
                            goto decode_err;
                        }
                    }
                    else if(ret != Z_BUF_ERROR)
                    {
                        ret = VPNG_EIDAT_STREAM;
                        goto decode_err;
                    }
                }

                /* We don't have scanline_width of data and we ran out of data for this chunk */
                if(stream.avail_out !=0 && stream.avail_in == 0)
                {
                    ret = next_chunk(dec, &chunk, &next, 0);
                    if(ret) goto decode_err;

                    memcpy(&chunk, &next, sizeof(struct vpng_chunk));

                    if(memcmp(chunk.type, type_idat, 4))
                    {
                        ret = VPNG_EIDAT_TOO_SHORT;
                        goto decode_err;
                    }

                    stream.avail_in = chunk.length;
                    stream.next_in = dec->data + chunk.offset + 8;
                }

            }while(stream.avail_out != 0);

            ret = defilter_scanline(prev_scanline, scanline, scanline_width, bytes_per_pixel);
            if(ret) goto decode_err;

            uint32_t k;
            uint8_t r, g, b, a, gray;
            uint16_t r16, g16, b16, a16, gray16;
            unsigned char pixel[4] = {0};
            uint8_t depth_target = 8;

            r=0; g=0; b=0; a=0; gray=0;
            r16=0; g16=0; b16=0; a16=0; gray16=0;

            scanline++; /* increment past filter byte, keep indexing 0-based */

            /* Process a scanline per-pixel and write to *out */
            for(k=0; k < sub[pass].width; k++)
            {
                /* Extract a pixel from the scanline, if bit_depth is 16
                   gray16, a16, r16, g16, b16 is set, otherwise gray, a, r, g, b
                   is set depending on colour type.
                */
                switch(dec->ihdr.colour_type)
                {
                    case VPNG_COLOUR_TYPE_GRAYSCALE:
                    {
                        if(dec->ihdr.bit_depth == 16)
                        {
                            memcpy(&gray16, scanline + (k * 2), 2);

                            gray16 = ntohs(gray16);

                            if(dec->have_trns && dec->trns_type0_grey_sample == gray16) a16 = 0;
                            else a16 = 65535;

                            gray16 = sample_to_target(gray16, 16, dec->sbit_type0_greyscale_bits, depth_target);
                            a16 = sample_to_target(a16, 16, 16, depth_target);
                        }
                        else /* <= 8 */
                        {
                            memcpy(&gray, scanline + k / (8 / dec->ihdr.bit_depth), 1);

                            uint16_t mask16 = (1 << dec->ihdr.bit_depth) - 1;
                            uint8_t mask = mask16; /* avoid shift by width */
                            uint8_t samples_per_byte = 8 / dec->ihdr.bit_depth;
                            uint8_t max_shift_amount = 8 - dec->ihdr.bit_depth;
                            uint8_t shift_amount = max_shift_amount - ((k % samples_per_byte) * dec->ihdr.bit_depth);

                            gray = gray & (mask << shift_amount);
                            gray = gray >> shift_amount;

                            if(dec->have_trns && (dec->trns_type0_grey_sample & 0x00FF) == gray) a = 0;
                            else a = 255;

                            gray = sample_to_target(gray, dec->ihdr.bit_depth, dec->sbit_type0_greyscale_bits, depth_target);
                            a = sample_to_target(a, 8, 8, depth_target);
                        }

                        break;
                    }
                    case VPNG_COLOUR_TYPE_TRUECOLOR:
                    {
                        if(dec->ihdr.bit_depth == 16)
                        {
                            memcpy(&r16, scanline + (k * 6), 2);
                            memcpy(&g16, scanline + (k * 6) + 2, 2);
                            memcpy(&b16, scanline + (k * 6) + 4, 2);

                            r16 = ntohs(r16);
                            g16 = ntohs(g16);
                            b16 = ntohs(b16);

                            if(dec->have_trns &&
                               dec->trns_type2.red == r16 &&
                               dec->trns_type2.green == g16 &&
                               dec->trns_type2.blue == b16) a16 = 0;
                            else a16 = 65535;

                            r16 = sample_to_target(r16, 16, dec->sbit_type2_3.red_bits, depth_target);
                            g16 = sample_to_target(g16, 16, dec->sbit_type2_3.green_bits, depth_target);
                            b16 = sample_to_target(b16, 16, dec->sbit_type2_3.blue_bits, depth_target);
                            a16 = sample_to_target(a16, 16, 16, depth_target);
                        }
                        else /* == 8 */
                        {
                            memcpy(&r, scanline + (k * 3), 1);
                            memcpy(&g, scanline + (k * 3) + 1, 1);
                            memcpy(&b, scanline + (k * 3) + 2, 1);

                            if(dec->have_trns &&
                              (dec->trns_type2.red & 0x00FF) == r &&
                              (dec->trns_type2.green & 0x00FF) == g &&
                              (dec->trns_type2.blue & 0x00FF) == b) a = 0;
                            else a = 255;

                            r = sample_to_target(r, 8, dec->sbit_type2_3.red_bits, depth_target);
                            g = sample_to_target(g, 8, dec->sbit_type2_3.green_bits, depth_target);
                            b = sample_to_target(b, 8, dec->sbit_type2_3.blue_bits, depth_target);
                            a = sample_to_target(a, 8, 8, depth_target);
                        }

                        break;
                    }
                    case VPNG_COLOUR_TYPE_INDEXED_COLOUR:
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

                        if(entry < dec->n_plte_entries)
                        {
                            r = dec->plte_entries[entry].red;
                            g = dec->plte_entries[entry].green;
                            b = dec->plte_entries[entry].blue;
                        }
                        else
                        {
                            ret = VPNG_EPLTE_IDX;
                            goto decode_err;
                        }

                        if(dec->have_trns &&
                           (entry < dec->n_trns_type3_entries)) a = dec->trns_type3_alpha[entry];
                        else a = 255;

                        r = sample_to_target(r, 8, 8, depth_target);
                        g = sample_to_target(g, 8, 8, depth_target);
                        b = sample_to_target(b, 8, 8, depth_target);
                        a = sample_to_target(a, 8, 8, depth_target);

                        break;
                    }
                    case VPNG_COLOUR_TYPE_GRAYSCALE_WITH_ALPHA:
                    {
                        if(dec->ihdr.bit_depth == 16)
                        {
                            memcpy(&gray16, scanline + (k * 4), 2);
                            memcpy(&a16, scanline + (k * 4) + 2, 2);

                            gray16 = ntohs(gray16);
                            a16 = ntohs(a16);

                            gray16 = sample_to_target(gray16, 16, dec->sbit_type4.greyscale_bits, depth_target);
                            a16 = sample_to_target(a16, 16, dec->sbit_type4.alpha_bits, depth_target);
                        }
                        else /* == 8 */
                        {
                            memcpy(&gray, scanline + (k * 2), 1);
                            memcpy(&a, scanline + (k * 2) + 1, 1);

                            gray = sample_to_target(gray, 8, dec->sbit_type4.greyscale_bits, depth_target);
                            a = sample_to_target(a, 8, dec->sbit_type4.alpha_bits, depth_target);
                        }

                        break;
                    }
                    case VPNG_COLOUR_TYPE_TRUECOLOR_WITH_ALPHA:
                    {
                        if(dec->ihdr.bit_depth == 16)
                        {
                            memcpy(&r16, scanline + (k * 8), 2);
                            memcpy(&g16, scanline + (k * 8) + 2, 2);
                            memcpy(&b16, scanline + (k * 8) + 4, 2);
                            memcpy(&a16, scanline + (k * 8) + 6, 2);

                            r16 = ntohs(r16);
                            g16 = ntohs(g16);
                            b16 = ntohs(b16);
                            a16 = ntohs(a16);

                            r16 = sample_to_target(r16, 16, dec->sbit_type6.red_bits, depth_target);
                            g16 = sample_to_target(g16, 16, dec->sbit_type6.green_bits, depth_target);
                            b16 = sample_to_target(b16, 16, dec->sbit_type6.blue_bits, depth_target);
                            a16 = sample_to_target(a16, 16, dec->sbit_type6.alpha_bits, depth_target);
                        }
                        else /* == 8 */
                        {
                            memcpy(&r, scanline + (k * 4), 1);
                            memcpy(&g, scanline + (k * 4) + 1, 1);
                            memcpy(&b, scanline + (k * 4) + 2, 1);
                            memcpy(&a, scanline + (k * 4) + 3, 1);

                            r = sample_to_target(r, 8, dec->sbit_type6.red_bits, depth_target);
                            g = sample_to_target(g, 8, dec->sbit_type6.green_bits, depth_target);
                            b = sample_to_target(b, 8, dec->sbit_type6.blue_bits, depth_target);
                            a = sample_to_target(a, 8, dec->sbit_type6.alpha_bits, depth_target);
                        }

                        break;
                    }
                }/* switch(dec->ihdr.colour_type) */

                if(dec->ihdr.bit_depth == 16)
                {
                    r = r16;
                    g = g16;
                    b = b16;
                    a = a16;
                    gray = gray16;
                }

                if(dec->ihdr.colour_type == VPNG_COLOUR_TYPE_GRAYSCALE ||
                   dec->ihdr.colour_type == VPNG_COLOUR_TYPE_GRAYSCALE_WITH_ALPHA)
                {
                    r = gray;
                    g = gray;
                    b = gray;
                }

                memcpy(pixel, &r, 1);
                memcpy(pixel + 1, &g, 1);
                memcpy(pixel + 2, &b, 1);
                memcpy(pixel + 3, &a, 1);

                size_t pixel_offset;
                size_t pixel_size = 4;

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


   if(stream.avail_in != 0)
    {
        ret = VPNG_EIDAT_TOO_LONG;
        goto decode_err;
    }

decode_err:

    inflateEnd(&stream);
    free(scanline_orig);
    free(prev_scanline);

    if(!ret)
    {
        memcpy(&dec->last_idat, &chunk, sizeof(struct vpng_chunk));
        ret = validate_past_idat(dec);
        if(ret) return ret;
    }

    return ret;
}

