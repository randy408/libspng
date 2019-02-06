#include "common.h"

#include <string.h>

#define SPNG_STR(x) _SPNG_STR(x)
#define _SPNG_STR(x) #x

#define SPNG_VERSION_STRING SPNG_STR(SPNG_VERSION_MAJOR) "." \
                            SPNG_STR(SPNG_VERSION_MINOR) "." \
                            SPNG_STR(SPNG_VERSION_PATCH)

const uint32_t png_u32max = 2147483647;
const int32_t png_s32min = -2147483647;

const uint8_t type_ihdr[4] = { 73, 72, 68, 82 };
const uint8_t type_plte[4] = { 80, 76, 84, 69 };
const uint8_t type_idat[4] = { 73, 68, 65, 84 };
const uint8_t type_iend[4] = { 73, 69, 78, 68 };

const uint8_t type_trns[4] = { 116, 82, 78, 83 };
const uint8_t type_chrm[4] = { 99,  72, 82, 77 };
const uint8_t type_gama[4] = { 103, 65, 77, 65 };
const uint8_t type_iccp[4] = { 105, 67, 67, 80 };
const uint8_t type_sbit[4] = { 115, 66, 73, 84 };
const uint8_t type_srgb[4] = { 115, 82, 71, 66 };
const uint8_t type_text[4] = { 116, 69, 88, 116 };
const uint8_t type_ztxt[4] = { 122, 84, 88, 116 };
const uint8_t type_itxt[4] = { 105, 84, 88, 116 };
const uint8_t type_bkgd[4] = { 98,  75, 71, 68 };
const uint8_t type_hist[4] = { 104, 73, 83, 84 };
const uint8_t type_phys[4] = { 112, 72, 89, 115 };
const uint8_t type_splt[4] = { 115, 80, 76, 84 };
const uint8_t type_time[4] = { 116, 73, 77, 69 };

const uint8_t type_offs[4] = { 111, 70, 70, 115 };
const uint8_t type_exif[4] = { 101, 88, 73, 102 };


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

int spng_set_crc_action(spng_ctx *ctx, int critical, int ancillary)
{
    if(ctx == NULL) return 1;

    if(critical > 2 || critical < 0) return 1;
    if(ancillary > 2 || critical < 0) return 1;

    if(critical == SPNG_CRC_DISCARD) return 1;

    ctx->crc_action_critical = critical;
    ctx->crc_action_ancillary = ancillary;

    return 0;
}

int spng_decoded_image_size(spng_ctx *ctx, int fmt, size_t *out)
{
    if(ctx == NULL || out == NULL) return 1;

    if(!ctx->valid_state) return SPNG_EBADSTATE;

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
    if(ctx == NULL || ihdr == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    memcpy(ihdr, &ctx->ihdr, sizeof(struct spng_ihdr));

    return 0;
}

int spng_get_plte(spng_ctx *ctx, struct spng_plte *plte)
{
    if(ctx == NULL || plte == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->stored_plte) return SPNG_ECHUNKAVAIL;

    memcpy(plte, &ctx->plte, sizeof(struct spng_plte));

    return 0;
}

int spng_get_trns(spng_ctx *ctx, struct spng_trns *trns)
{
    if(ctx == NULL || trns == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->stored_trns) return SPNG_ECHUNKAVAIL;

    memcpy(trns, &ctx->trns, sizeof(struct spng_trns));

    return 0;
}

int spng_get_chrm(spng_ctx *ctx, struct spng_chrm *chrm)
{
    if(ctx == NULL || chrm == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

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
    if(ctx == NULL || chrm == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->stored_chrm) return SPNG_ECHUNKAVAIL;

    memcpy(chrm, &ctx->chrm_int, sizeof(struct spng_chrm_int));

    return 0;
}

int spng_get_gama(spng_ctx *ctx, double *gamma)
{
    if(ctx == NULL || gamma == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->stored_gama) return SPNG_ECHUNKAVAIL;

    *gamma = (double)ctx->gama / 100000.0;

    return 0;
}

int spng_get_iccp(spng_ctx *ctx, struct spng_iccp *iccp)
{
    if(ctx == NULL || iccp == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->stored_iccp) return SPNG_ECHUNKAVAIL;

    memcpy(iccp, &ctx->iccp, sizeof(struct spng_iccp));

    return 0;
}

int spng_get_sbit(spng_ctx *ctx, struct spng_sbit *sbit)
{
    if(ctx == NULL || sbit == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->stored_sbit) return SPNG_ECHUNKAVAIL;

    memcpy(sbit, &ctx->sbit, sizeof(struct spng_sbit));

    return 0;
}

int spng_get_srgb(spng_ctx *ctx, uint8_t *rendering_intent)
{
    if(ctx == NULL || rendering_intent == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

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
    if(ctx == NULL || bkgd == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->stored_bkgd) return SPNG_ECHUNKAVAIL;

    memcpy(bkgd, &ctx->bkgd, sizeof(struct spng_bkgd));

    return 0;
}

int spng_get_hist(spng_ctx *ctx, struct spng_hist *hist)
{
    if(ctx == NULL || hist == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->stored_hist) return SPNG_ECHUNKAVAIL;

    memcpy(hist, &ctx->hist, sizeof(struct spng_hist));

    return 0;
}

int spng_get_phys(spng_ctx *ctx, struct spng_phys *phys)
{
    if(ctx == NULL || phys == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

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
    if(ctx == NULL || time == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->stored_time) return SPNG_ECHUNKAVAIL;

    memcpy(time, &ctx->time, sizeof(struct spng_time));

    return 0;
}

int spng_get_offs(spng_ctx *ctx, struct spng_offs *offs)
{
    if(ctx == NULL || offs == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->stored_offs) return SPNG_ECHUNKAVAIL;

    memcpy(offs, &ctx->offs, sizeof(struct spng_offs));

    return 0;
}

int spng_get_exif(spng_ctx *ctx, struct spng_exif *exif)
{
    if(ctx == NULL || exif == NULL) return 1;

    int ret = get_ancillary(ctx);
    if(ret) return ret;

    if(!ctx->stored_exif) return SPNG_ECHUNKAVAIL;

    memcpy(exif, &ctx->exif, sizeof(struct spng_exif));

    return 0;
}

int spng_set_ihdr(spng_ctx *ctx, struct spng_ihdr *ihdr)
{
    if(ctx == NULL || ihdr == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

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
    if(ctx == NULL || plte == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

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
    if(ctx == NULL || trns == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

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
    if(ctx == NULL || chrm == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

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
    if(ctx == NULL || chrm_int == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    if(check_chrm_int(chrm_int)) return SPNG_ECHRM;

    memcpy(&ctx->chrm_int, chrm_int, sizeof(struct spng_chrm_int));

    ctx->stored_chrm = 1;
    ctx->user_chrm = 1;

    return 0;
}

int spng_set_gama(spng_ctx *ctx, double gamma)
{
    if(ctx == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

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
    if(ctx == NULL || iccp == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

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
    if(ctx == NULL || sbit == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    if(!ctx->stored_ihdr) return 1;
    if(check_sbit(sbit, &ctx->ihdr)) return 1;

    memcpy(&ctx->sbit, sbit, sizeof(struct spng_sbit));

    ctx->stored_sbit = 1;
    ctx->user_sbit = 1;

    return 0;
}

int spng_set_srgb(spng_ctx *ctx, uint8_t rendering_intent)
{
    if(ctx == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

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
    if(ctx == NULL || bkgd == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

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
    if(ctx == NULL || hist == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    if(!ctx->stored_plte) return SPNG_EHIST_NO_PLTE;

    memcpy(&ctx->hist, hist, sizeof(struct spng_hist));

    ctx->stored_hist = 1;
    ctx->user_hist = 1;

    return 0;
}

int spng_set_phys(spng_ctx *ctx, struct spng_phys *phys)
{
    if(ctx == NULL || phys == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

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
    if(ctx == NULL || time == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    if(check_time(time)) return SPNG_ETIME;

    memcpy(&ctx->time, time, sizeof(struct spng_time));

    ctx->stored_time = 1;
    ctx->user_time = 1;

    return 0;
}

int spng_set_offs(spng_ctx *ctx, struct spng_offs *offs)
{
    if(ctx == NULL || offs == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

    if(check_offs(offs)) return SPNG_EOFFS;

    memcpy(&ctx->offs, offs, sizeof(struct spng_offs));

    ctx->stored_offs = 1;
    ctx->user_offs = 1;

    return 0;
}

int spng_set_exif(spng_ctx *ctx, struct spng_exif *exif)
{
    if(ctx == NULL || exif == NULL) return 1;

    int ret = get_ancillary2(ctx);
    if(ret) return ret;

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
