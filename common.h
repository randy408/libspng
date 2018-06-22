#ifndef SPNG_COMMON_H
#define SPNG_COMMON_H

#include "spng.h"

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

int get_ancillary(struct spng_ctx *ctx);

int check_ihdr(struct spng_ihdr *ihdr, uint32_t max_width, uint32_t max_height);
int check_sbit(struct spng_sbit *sbit, struct spng_ihdr *ihdr);
int check_chrm(struct spng_chrm *chrm);
int check_time(struct spng_time *time);

int check_png_keyword(const char str[80]);

#endif /* SPNG_COMMON_H */
