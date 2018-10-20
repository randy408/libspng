#ifndef SPNG_COMMON_H
#define SPNG_COMMON_H

#include "spng.h"

#define SPNG_FILTER_TYPE_NONE 0
#define SPNG_FILTER_TYPE_SUB 1
#define SPNG_FILTER_TYPE_UP 2
#define SPNG_FILTER_TYPE_AVERAGE 3
#define SPNG_FILTER_TYPE_PAETH 4

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
    unsigned user_time: 1;
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


struct spng_subimage
{
    uint32_t width;
    uint32_t height;
    size_t scanline_width;
};

int get_ancillary(struct spng_ctx *ctx);
int get_ancillary2(struct spng_ctx *ctx);

int calculate_subimages(struct spng_subimage sub[7], size_t *widest_scanline, struct spng_ihdr *ihdr, int channels);

int check_ihdr(struct spng_ihdr *ihdr, uint32_t max_width, uint32_t max_height);
int check_sbit(struct spng_sbit *sbit, struct spng_ihdr *ihdr);
int check_chrm(struct spng_chrm *chrm);
int check_phys(struct spng_phys *phys);
int check_time(struct spng_time *time);
int check_offs(struct spng_offs *offs);
int check_exif(struct spng_exif *exif);

int check_png_keyword(const char str[80]);
int check_png_text(const char *str, size_t len);

#endif /* SPNG_COMMON_H */
