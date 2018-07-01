#include "common.h"

#include <string.h>

static const uint32_t png_u32max = 2147483647;
static const int32_t png_s32max = 2147483647;
static const int32_t png_s32min = -2147483647;

int check_ihdr(struct spng_ihdr *ihdr, uint32_t max_width, uint32_t max_height)
{
    if(ihdr->width > png_u32max || ihdr->width > max_width) return SPNG_EWIDTH;
    if(ihdr->height > png_u32max || ihdr->height > max_height) return SPNG_EHEIGHT;

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

int check_sbit(struct spng_sbit *sbit, struct spng_ihdr *ihdr)
{
    if(sbit == NULL || ihdr == NULL) return 1;

    if(ihdr->colour_type == 0)
    {
        if(sbit->greyscale_bits == 0) return SPNG_ESBIT;
        if(sbit->greyscale_bits > ihdr->bit_depth) return SPNG_ESBIT;
    }
    else if(ihdr->colour_type == 2 || ihdr->colour_type == 3)
    {
        if(sbit->red_bits == 0) return SPNG_ESBIT;
        if(sbit->green_bits == 0) return SPNG_ESBIT;
        if(sbit->blue_bits == 0) return SPNG_ESBIT;

        uint8_t bit_depth;
        if(ihdr->colour_type == 3) bit_depth = 8;
        else bit_depth = ihdr->bit_depth;

        if(sbit->red_bits > bit_depth) return SPNG_ESBIT;
        if(sbit->green_bits > bit_depth) return SPNG_ESBIT;
        if(sbit->blue_bits > bit_depth) return SPNG_ESBIT;
    }
    else if(ihdr->colour_type == 4)
    {
        if(sbit->greyscale_bits == 0) return SPNG_ESBIT;
        if(sbit->greyscale_bits > ihdr->bit_depth) return SPNG_ESBIT;
    }
    else if(ihdr->colour_type == 6)
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

int check_chrm(struct spng_chrm *chrm)
{
    if(chrm == NULL) return 1;

    if(chrm->white_point_x > png_u32max ||
       chrm->white_point_y > png_u32max ||
       chrm->red_x > png_u32max ||
       chrm->red_y > png_u32max ||
       chrm->green_x  > png_u32max ||
       chrm->green_y  > png_u32max ||
       chrm->blue_x > png_u32max ||
       chrm->blue_y > png_u32max) return SPNG_ECHRM;

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

    if(offs->x > png_s32max || offs->x > png_s32max) return 1;
    if(offs->x < png_s32min || offs->y < png_s32min) return 1;
    if(offs->unit_specifier > 1) return 1;

    return 0;
}

int check_exif(struct spng_exif *exif)
{
    if(exif == NULL) return 1;

    if(exif->length < 4) return SPNG_ECHUNK_SIZE;

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

        if( (c >= 32 && c <= 126) || (c >= 161 && c <= 255) ) str++;
        else return 1; /* invalid character */
    }

    return 0;
}

int spng_get_ihdr(struct spng_ctx *ctx, struct spng_ihdr *ihdr)
{
    if(ctx == NULL || ihdr == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    memcpy(ihdr, &ctx->ihdr, sizeof(struct spng_ihdr));

    return 0;
}

int spng_get_plte(struct spng_ctx *ctx, struct spng_plte *plte)
{
    if(ctx == NULL || plte == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->have_plte) return SPNG_ECHUNKAVAIL;

    memcpy(plte, &ctx->plte, sizeof(struct spng_plte));

    return 0;
}

int spng_get_trns(struct spng_ctx *ctx, struct spng_trns *trns)
{
    if(ctx == NULL || trns == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->have_trns) return SPNG_ECHUNKAVAIL;

    memcpy(trns, &ctx->trns, sizeof(struct spng_trns));

    return 0;
}

int spng_get_chrm(struct spng_ctx *ctx, struct spng_chrm *chrm)
{
    if(ctx == NULL || chrm == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->have_chrm) return SPNG_ECHUNKAVAIL;

    memcpy(chrm, &ctx->chrm, sizeof(struct spng_chrm));

    return 0;
}

int spng_get_gama(struct spng_ctx *ctx, double *gamma)
{
    if(ctx == NULL || gamma == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->have_gama) return SPNG_ECHUNKAVAIL;

    *gamma = (double)ctx->gama / 100000.0;

    return 0;
}

int spng_get_iccp(struct spng_ctx *ctx, struct spng_iccp *iccp)
{
    if(ctx == NULL || iccp == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->have_iccp) return SPNG_ECHUNKAVAIL;

    memcpy(iccp, &ctx->iccp, sizeof(struct spng_iccp));

    return 0;
}

int spng_get_sbit(struct spng_ctx *ctx, struct spng_sbit *sbit)
{
    if(ctx == NULL || sbit == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->have_sbit) return SPNG_ECHUNKAVAIL;

    memcpy(sbit, &ctx->sbit, sizeof(struct spng_sbit));

    return 0;
}

int spng_get_srgb(struct spng_ctx *ctx, uint8_t *rendering_intent)
{
    if(ctx == NULL || rendering_intent == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->have_srgb) return SPNG_ECHUNKAVAIL;

    *rendering_intent = ctx->srgb_rendering_intent;

    return 0;
}

int spng_get_bkgd(struct spng_ctx *ctx, struct spng_bkgd *bkgd)
{
    if(ctx == NULL || bkgd == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->have_bkgd) return SPNG_ECHUNKAVAIL;

    memcpy(bkgd, &ctx->bkgd, sizeof(struct spng_bkgd));

    return 0;
}

int spng_get_hist(struct spng_ctx *ctx, struct spng_hist *hist)
{
    if(ctx == NULL || hist == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->have_hist) return SPNG_ECHUNKAVAIL;

    memcpy(hist, &ctx->hist, sizeof(struct spng_hist));

    return 0;
}

int spng_get_phys(struct spng_ctx *ctx, struct spng_phys *phys)
{
    if(ctx == NULL || phys == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->have_phys) return SPNG_ECHUNKAVAIL;

    memcpy(phys, &ctx->phys, sizeof(struct spng_phys));

    return 0;
}

int spng_get_time(struct spng_ctx *ctx, struct spng_time *time)
{
    if(ctx == NULL || time == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->have_time) return SPNG_ECHUNKAVAIL;

    memcpy(time, &ctx->time, sizeof(struct spng_time));

    return 0;
}

int spng_get_offs(struct spng_ctx *ctx, struct spng_offs *offs)
{
    if(ctx == NULL || offs == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->have_offs) return SPNG_ECHUNKAVAIL;

    memcpy(offs, &ctx->offs, sizeof(struct spng_offs));

    return 0;
}

int spng_get_exif(struct spng_ctx *ctx, struct spng_exif *exif)
{
    if(ctx == NULL || exif == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->have_exif) return SPNG_ECHUNKAVAIL;

    memcpy(exif, &ctx->exif, sizeof(struct spng_exif));

    return 0;
}

int spng_set_ihdr(struct spng_ctx *ctx, struct spng_ihdr *ihdr)
{
    if(ctx == NULL || ihdr == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    if(ctx->have_ihdr) return 1;

    ret = check_ihdr(ihdr, ctx->max_width, ctx->max_height);
    if(ret) return ret;

    memcpy(&ctx->ihdr, ihdr, sizeof(struct spng_ihdr));

    ctx->have_ihdr = 1;

    return 0;
}

int spng_set_plte(struct spng_ctx *ctx, struct spng_plte *plte)
{
    if(ctx == NULL || plte == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    if(!ctx->have_ihdr) return 1;

    if(plte->n_entries > 256) return 1;

    if(ctx->ihdr.colour_type == 3)
    {
        if(plte->n_entries > (1 << ctx->ihdr.bit_depth)) return 1;
    }

    memcpy(&ctx->plte, plte, sizeof(struct spng_plte));

    ctx->have_plte = 1;

    return 0;
}

int spng_set_trns(struct spng_ctx *ctx, struct spng_trns *trns)
{
    if(ctx == NULL || trns == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    if(!ctx->have_ihdr) return 1;

    uint16_t mask = ~0;
    if(ctx->ihdr.bit_depth < 16) mask = (1 << ctx->ihdr.bit_depth) - 1;

    if(ctx->ihdr.colour_type == 0)
    {
        trns->type0_grey_sample &= mask;
    }
    else if(ctx->ihdr.colour_type == 2)
    {
        trns->type2.red &= mask;
        trns->type2.green &= mask;
        trns->type2.blue &= mask;
    }
    else if(ctx->ihdr.colour_type == 3)
    {
        if(!ctx->have_plte) return 1;
    }
    else return SPNG_ETRNS_COLOUR_TYPE;

    memcpy(&ctx->trns, trns, sizeof(struct spng_trns));

    ctx->have_trns = 1;

    return 0;
}

int spng_set_chrm(struct spng_ctx *ctx, struct spng_chrm *chrm)
{
    if(ctx == NULL || chrm == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    if(check_chrm(chrm)) return SPNG_ECHRM;

    memcpy(&ctx->chrm, chrm, sizeof(struct spng_chrm));

    ctx->have_chrm = 1;

    return 0;
}

int spng_set_gama(struct spng_ctx *ctx, double gamma)
{
    if(ctx == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    uint32_t gama = gamma * 100000.0;

    if(gama > png_u32max) return 1;

    ctx->gama = gama;

    ctx->have_gama = 1;

    return 0;
}

int spng_set_iccp(struct spng_ctx *ctx, struct spng_iccp *iccp)
{
    if(ctx == NULL || iccp == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    memcpy(&ctx->iccp, iccp, sizeof(struct spng_iccp));

    ctx->have_iccp = 1;

    return 0;
}

int spng_set_sbit(struct spng_ctx *ctx, struct spng_sbit *sbit)
{
    if(ctx == NULL || sbit == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    if(!ctx->have_ihdr) return 1;
    if(check_sbit(sbit, &ctx->ihdr)) return 1;

    memcpy(&ctx->sbit, sbit, sizeof(struct spng_sbit));

    ctx->have_sbit = 1;

    return 0;
}

int spng_set_srgb(struct spng_ctx *ctx, uint8_t rendering_intent)
{
    if(ctx == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    if(rendering_intent > 3) return 1;

    ctx->srgb_rendering_intent = rendering_intent;

    ctx->have_srgb = 1;

    return 0;
}

int spng_set_bkgd(struct spng_ctx *ctx, struct spng_bkgd *bkgd)
{
    if(ctx == NULL || bkgd == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    if(!ctx->have_ihdr) return 1;

    uint16_t mask = ~0;

    if(ctx->ihdr.bit_depth < 16) mask = (1 << ctx->ihdr.bit_depth) - 1;

    if(ctx->ihdr.colour_type == 0 || ctx->ihdr.colour_type == 4)
    {
        bkgd->type0_4_greyscale &= mask;
    }
    else if(ctx->ihdr.colour_type == 2 || ctx->ihdr.colour_type == 6)
    {
        bkgd->type2_6.red &= mask;
        bkgd->type2_6.green &= mask;
        bkgd->type2_6.blue &= mask;
    }
    else if(ctx->ihdr.colour_type == 3)
    {
        if(!ctx->have_plte) return SPNG_EBKGD_NO_PLTE;
        if(bkgd->type3_plte_index >= ctx->plte.n_entries) return SPNG_EBKGD_PLTE_IDX;
    }

    memcpy(&ctx->bkgd, bkgd, sizeof(struct spng_bkgd));

    ctx->have_bkgd = 1;

    return 0;
}

int spng_set_hist(struct spng_ctx *ctx, struct spng_hist *hist)
{
    if(ctx == NULL || hist == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    if(!ctx->have_plte) return SPNG_EHIST_NO_PLTE;

    memcpy(&ctx->hist, hist, sizeof(struct spng_hist));

    ctx->have_hist = 1;

    return 0;
}

int spng_set_phys(struct spng_ctx *ctx, struct spng_phys *phys)
{
    if(ctx == NULL || phys == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    if(check_phys(phys)) return SPNG_EPHYS;

    memcpy(&ctx->phys, phys, sizeof(struct spng_phys));

    ctx->have_phys = 1;

    return 0;
}

int spng_set_time(struct spng_ctx *ctx, struct spng_time *time)
{
    if(ctx == NULL || time == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    if(check_time(time)) return SPNG_ETIME;

    memcpy(&ctx->time, time, sizeof(struct spng_time));

    ctx->have_time = 1;

    return 0;
}

int spng_set_offs(struct spng_ctx *ctx, struct spng_offs *offs)
{
    if(ctx == NULL || offs == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    if(check_offs(offs)) return SPNG_EOFFS;

    memcpy(&ctx->offs, offs, sizeof(struct spng_offs));

    ctx->have_offs = 1;

    return 0;
}

int spng_set_exif(struct spng_ctx *ctx, struct spng_exif *exif)
{
    if(ctx == NULL || exif == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    if(check_exif(exif)) return SPNG_EEXIF;

    memcpy(&ctx->exif, exif, sizeof(struct spng_exif));

    ctx->have_exif = 1;

    return 0;
}

