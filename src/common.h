#ifndef SPNG_COMMON_H
#define SPNG_COMMON_H

#include "spng.h"

#include <limits.h>
#include <string.h>
#include <math.h>

#include <zlib.h>

#if defined(SPNG_OPTIMIZE_FILTER)
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

const uint32_t png_u32max;
const int32_t png_s32min;

const uint8_t type_ihdr[4];
const uint8_t type_plte[4];
const uint8_t type_idat[4];
const uint8_t type_iend[4];
const uint8_t type_trns[4];
const uint8_t type_chrm[4];
const uint8_t type_gama[4];
const uint8_t type_iccp[4];
const uint8_t type_sbit[4];
const uint8_t type_srgb[4];
const uint8_t type_text[4]; 
const uint8_t type_ztxt[4]; 
const uint8_t type_itxt[4]; 
const uint8_t type_bkgd[4];
const uint8_t type_hist[4];
const uint8_t type_phys[4]; 
const uint8_t type_splt[4];
const uint8_t type_time[4];
const uint8_t type_offs[4]; 
const uint8_t type_exif[4]; 

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

#endif /* SPNG_COMMON_H */
