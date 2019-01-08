#include "common.h"

struct spng_decomp
{
    unsigned char *buf; /* input buffer */
    size_t size; /* input buffer size */

    char *out; /* output buffer */
    size_t decomp_size; /* decompressed data size */
    size_t decomp_alloc_size; /* actual buffer size */

    size_t initial_size; /* initial value for decomp_size */
};

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
            uint32_t len = 8192;

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
    if(memcmp(ctx->current_chunk.type, type_idat, 4)) return 1;

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
        len = 8192;
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

static int defilter_scanline(const unsigned char *prev_scanline, unsigned char *scanline,
                             size_t scanline_width, uint8_t bytes_per_pixel)
{
    if(prev_scanline == NULL || scanline == NULL || scanline_width <= 1) return 1;

    size_t i;
    uint8_t filter = 0;
    memcpy(&filter, scanline, 1);

    if(filter > 4) return SPNG_EFILTER;
    if(filter == 0) return 0;

#if defined(SPNG_OPTIMIZE_DEFILTER)
    if(filter == SPNG_FILTER_TYPE_UP) goto no_opt;

    if(bytes_per_pixel == 4)
    {
        if(filter == SPNG_FILTER_TYPE_SUB)
            png_read_filter_row_sub4(scanline_width - 1, scanline + 1);
        else if(filter == SPNG_FILTER_TYPE_AVERAGE)
            png_read_filter_row_avg4(scanline_width - 1, scanline + 1, prev_scanline + 1);
        else if(filter == SPNG_FILTER_TYPE_PAETH)
            png_read_filter_row_paeth4(scanline_width - 1, scanline + 1, prev_scanline + 1);

        return 0;
    }
    else if(bytes_per_pixel == 3)
    {
        if(filter == SPNG_FILTER_TYPE_SUB)
            png_read_filter_row_sub3(scanline_width - 1, scanline + 1);
        else if(filter == SPNG_FILTER_TYPE_AVERAGE)
            png_read_filter_row_avg3(scanline_width - 1, scanline + 1, prev_scanline + 1);
        else if(filter == SPNG_FILTER_TYPE_PAETH)
            png_read_filter_row_paeth3(scanline_width - 1, scanline + 1, prev_scanline + 1);

        return 0;
    }
no_opt:
#endif

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
static int decompress_zstream(spng_ctx *ctx, struct spng_decomp *params)
{
    if(params == NULL) return 1;
    if(params->buf == NULL || params->size == 0) return 1;
    if(params->initial_size == 0) return 1;

    params->decomp_alloc_size = params->initial_size;
    params->out = spng__malloc(ctx, params->decomp_alloc_size);
    if(params->out == NULL) return 1;

    int ret;
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    if(inflateInit(&stream) != Z_OK)
    {
        spng__free(ctx, params->out);
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
            spng__free(ctx, params->out);
            return SPNG_EZLIB;
        }

        if(stream.avail_out == 0 && stream.avail_in != 0)
        {
            if(2 > SIZE_MAX / params->decomp_alloc_size)
            {
                inflateEnd(&stream);
                spng__free(ctx, params->out);
                return SPNG_EOVERFLOW;
            }

            params->decomp_alloc_size *= 2;
            void *temp = spng__realloc(ctx, params->out, params->decomp_alloc_size);

            if(temp == NULL)
            {
                inflateEnd(&stream);
                spng__free(ctx, params->out);
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

/* Read and validate tEXt, zTXt, iTXt chunks */
static int get_text(spng_ctx *ctx, unsigned char *data, struct spng_chunk *chunk)
{
    if(ctx == NULL || data == NULL || chunk == NULL) return 1;

    if(!chunk->length) return SPNG_ECHUNK_SIZE;

    if(!ctx->stored_text)
    {
        ctx->n_text = 1;
        ctx->text_list = spng__calloc(ctx, 1, sizeof(struct spng_text));
        if(ctx->text_list == NULL) return SPNG_EMEM;
    }
    else
    {
        ctx->n_text++;
        if(ctx->n_text < 1) return SPNG_EOVERFLOW;
        if(sizeof(struct spng_text) > SIZE_MAX / ctx->n_text) return SPNG_EOVERFLOW;

        void *buf = spng__realloc(ctx, ctx->text_list, ctx->n_text * sizeof(struct spng_text));
        if(buf == NULL) return SPNG_EMEM;
        ctx->text_list = buf;
    }

    struct spng_text *text;
    struct spng_decomp params = { 0 };
    uint32_t i = ctx->n_text - 1;

    memset(&ctx->text_list[i], 0, sizeof(struct spng_text));
    text = &ctx->text_list[i];

    size_t keyword_len = chunk->length > 80 ? 80 : chunk->length;
    char *keyword_nul = memchr(data, '\0', keyword_len);
    if(keyword_nul == NULL) return SPNG_ETEXT_KEYWORD;

    memcpy(&text->keyword, data, keyword_len);

    if(check_png_keyword(text->keyword)) return SPNG_ETEXT_KEYWORD;

    keyword_len = strlen(text->keyword);

    if(!memcmp(chunk->type, type_text, 4))
    {
        text->type = SPNG_TEXT;
        text->length = chunk->length - keyword_len - 1;
        if(text->length == 0) return SPNG_ETEXT;

        /* one byte extra for nul, text is not terminated */
        text->text = spng__malloc(ctx, text->length + 1);
        if(text->text == NULL) return SPNG_EMEM;

        memcpy(text->text, data + keyword_len + 1, text->length);
        text->text[text->length] = 0;

        /* text could end up being nul-terminated twice,
           make sure we have the right size */
        text->length = strlen(text->text);

        if(check_png_text(text->text, text->length)) return SPNG_ETEXT;
    }
    else if(!memcmp(chunk->type, type_ztxt, 4))
    {
        text->type = SPNG_ZTXT;
        if((chunk->length - keyword_len) < 2) return SPNG_EZTXT;

        memcpy(&text->compression_method, data + keyword_len + 1, 1);

        if(text->compression_method) return SPNG_EZTXT_COMPRESSION_METHOD;

        params.buf = data + keyword_len + 2;
        params.size = chunk->length - keyword_len - 2;
        params.initial_size = 1024;

        int ret = decompress_zstream(ctx, &params);
        if(ret) return ret;

        text->text = params.out;
        text->length = params.decomp_size;

        if(check_png_text(text->text, text->length)) return SPNG_EZTXT;
    }
    else if(!memcmp(chunk->type, type_itxt, 4))
    {
        text->type = SPNG_ITXT;
        if((chunk->length - keyword_len) < 3) return SPNG_EITXT;

        uint8_t compression_method;
        memcpy(&text->compression_flag, data + keyword_len + 1, 1);
        memcpy(&compression_method, data + keyword_len + 2, 1);

        if(compression_method) return SPNG_EITXT_COMPRESSION_METHOD;

        size_t max_len = chunk->length - keyword_len - 3;
        size_t lang_tag_len = max_len;

        char *lang_tag_nul = memchr(data + keyword_len + 3, '\0', lang_tag_len);
        if(lang_tag_nul == NULL) return SPNG_EITXT_LANG_TAG;

        lang_tag_len = strlen((char*)data + keyword_len + 3);

        text->language_tag = spng__malloc(ctx, lang_tag_len + 1);
        if(text->language_tag == NULL) return SPNG_EMEM;

        max_len -= lang_tag_len - 1;
        size_t translated_key_len = max_len;

        char *translated_key_nul = memchr(data + keyword_len + 3 + lang_tag_len + 1, '\0', translated_key_len);
        if(translated_key_nul == NULL) return SPNG_EITXT_TRANSLATED_KEY;

        translated_key_len = strlen((char*)data + keyword_len + 3 + lang_tag_len + 1);

        text->translated_keyword = spng__malloc(ctx, translated_key_len + 1);
        if(text->translated_keyword == NULL) return SPNG_EMEM;

        max_len -= translated_key_len - 1;
        size_t text_len = max_len;

        if(text->compression_flag)
        {
            params.buf = data + keyword_len + 2;
            params.size = chunk->length - keyword_len - 2;
            params.initial_size = 1024;

            int ret = decompress_zstream(ctx, &params);
            if(ret) return ret;

            text->text = params.out;
        }
        else
        {
            text->text = spng__malloc(ctx, text_len);
            if(text->text == NULL) return SPNG_EMEM;

            memcpy(text->text, data + keyword_len + 3 + lang_tag_len + translated_key_len + 2, text_len);
        }

    }
    else return 1;


    if(!memcmp(chunk->type, type_ztxt, 4) || ( !memcmp(chunk->type, type_itxt, 4) && text->compression_flag) )
    {
        /* nul-terminate */
        params.decomp_size++;
        if(params.decomp_size < 1) return SPNG_EOVERFLOW;

        if(params.decomp_alloc_size == params.decomp_size - 1)
        {
            void *temp = spng__realloc(ctx, text->text, params.decomp_size);
            if(temp == NULL) return SPNG_EMEM;
            text->text = temp;
        }

        text->text[params.decomp_size - 1] = '\0';

        text->length = strlen(text->text);
    }

    return 0;
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

            size_t name_len = chunk.length > 80 ? 80 : chunk.length;
            char *name_nul = memchr(data, '\0', name_len);
            if(name_nul == NULL) return SPNG_EICCP_NAME;

            memcpy(&ctx->iccp.profile_name, data, name_len);

            if(check_png_keyword(ctx->iccp.profile_name)) return SPNG_EICCP_NAME;

            name_len = strlen(ctx->iccp.profile_name);

            /* check for at least 2 bytes after profile name */
            if( (chunk.length - name_len - 1) < 2) return SPNG_ECHUNK_SIZE;

            uint8_t compression_method;
            memcpy(&compression_method, data + name_len + 1, 1);
            if(compression_method) return SPNG_EICCP_COMPRESSION_METHOD;

            struct spng_decomp params;
            params.buf = data + name_len + 2;
            params.size = chunk.length - name_len - 2;
            params.initial_size = 1024;

            ret = decompress_zstream(ctx, &params);
            if(ret) return ret;

            ctx->iccp.profile = params.out;
            ctx->iccp.profile_len = params.decomp_size;

            ctx->stored_iccp = 1;
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

            if(!ctx->stored_splt)
            {
                ctx->n_splt = 1;
                ctx->splt_list = spng__malloc(ctx, sizeof(struct spng_splt));
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

            ctx->file_splt = 1;
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

            if(ctx->user_text) continue;

            int ret = get_text(ctx, data, &chunk);
            if(ret) return ret;

            ctx->stored_text = 1;
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

            struct spng_exif exif;

            exif.data = spng__malloc(ctx, chunk.length);
            if(exif.data == NULL) return SPNG_EMEM;

            memcpy(exif.data, data, chunk.length);
            exif.length = chunk.length;

            if(check_exif(&exif)) return SPNG_EEXIF;

            ctx->file_exif = 1;

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

            if(ctx->user_text) continue;

            int ret = get_text(ctx, data, &chunk);
            if(ret) return ret;

            ctx->stored_text = 1;
        }
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

    unsigned char *scanline_orig = NULL, *scanline = NULL, *prev_scanline = NULL;

    scanline_orig = spng__malloc(ctx, scanline_width);
    prev_scanline = spng__malloc(ctx, scanline_width);

    if(scanline_orig == NULL || prev_scanline == NULL)
    {
        ret = SPNG_EMEM;
        goto decode_err;
    }

    /* Some of the error-handling goto's might leave scanline incremented,
       leading to a failed free(), this prevents that. */
    scanline = scanline_orig;

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

    if(flags & SPNG_DECODE_USE_GAMA && ctx->stored_gama)
    {
        float file_gamma = (float)ctx->gama / 100000.0f;
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

        if(ctx->lut_entries != lut_entries)
        {
            if(ctx->gamma_lut != NULL) spng__free(ctx, ctx->gamma_lut);

            ctx->gamma_lut = spng__malloc(ctx, lut_entries * sizeof(uint16_t));
            if(ctx->gamma_lut == NULL)
            {
                ret = SPNG_EMEM;
                goto decode_err;
            }
        }

        for(i=0; i < lut_entries; i++)
        {
            float screen_gamma = 2.2f;
            float exp = 1.0f / (file_gamma * screen_gamma);
            float c = pow((float)i / max, exp) * max;

            ctx->gamma_lut[i] = c;
        }

        ctx->lut_entries = lut_entries;
    }

    uint8_t red_sbits, green_sbits, blue_sbits, alpha_sbits, grayscale_sbits;

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

    uint8_t depth_target = 8; /* FMT_RGBA8 */
    if(fmt == SPNG_FMT_RGBA16) depth_target = 16;

    if(!ctx->streaming && ctx->have_last_idat)
    {
        ctx->data = ctx->png_buf + ctx->first_idat.offset;
        ctx->bytes_left = ctx->data_size - ctx->first_idat.offset;
        ctx->last_read_size = 8;
    }

    uint32_t bytes_read;

    ret = get_idat_bytes(ctx, &bytes_read);
    if(ret) goto decode_err;

    stream.avail_in = bytes_read;
    stream.next_in = ctx->data;


    int pass;
    uint32_t scanline_idx;
    for(pass=0; pass < 7; pass++)
    {
        /* Skip empty passes */
        if(sub[pass].width == 0 || sub[pass].height == 0) continue;

        scanline_width = sub[pass].scanline_width;

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

            ret = defilter_scanline(prev_scanline, scanline, scanline_width, bytes_per_pixel);
            if(ret) goto decode_err;

            uint32_t k;
            uint8_t r_8, g_8, b_8, a_8, gray_8;
            uint16_t r_16, g_16, b_16, a_16, gray_16;
            uint16_t r, g, b, a, gray;
            unsigned char pixel[8] = {0};
            size_t pixel_size = 4;
            size_t pixel_offset;

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
                        memcpy(&entry, scanline + k / (8 / ctx->ihdr.bit_depth), 1);

                        uint16_t mask16 = (1 << ctx->ihdr.bit_depth) - 1;
                        uint8_t mask = mask16; /* avoid shift by width */
                        uint8_t samples_per_byte = 8 / ctx->ihdr.bit_depth;
                        uint8_t max_shift_amount = 8 - ctx->ihdr.bit_depth;
                        uint8_t shift_amount = max_shift_amount - ((k % samples_per_byte) * ctx->ihdr.bit_depth);

                        entry = entry & (mask << shift_amount);
                        entry = entry >> shift_amount;

                        if(entry < ctx->plte.n_entries)
                        {
                            r_8 = ctx->plte.entries[entry].red;
                            g_8 = ctx->plte.entries[entry].green;
                            b_8 = ctx->plte.entries[entry].blue;
                        }
                        else
                        {
                            ret = SPNG_EPLTE_IDX;
                            goto decode_err;
                        }

                        if(flags & SPNG_DECODE_USE_TRNS && ctx->stored_trns &&
                           (entry < ctx->trns.n_type3_entries)) a_8 = ctx->trns.type3_alpha[entry];
                        else a_8 = 255;

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
                    uint8_t processing_depth = ctx->ihdr.bit_depth;
                    if(ctx->ihdr.color_type == SPNG_COLOR_TYPE_INDEXED) processing_depth = 8;

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
                    r = ctx->gamma_lut[r];
                    g = ctx->gamma_lut[g];
                    b = ctx->gamma_lut[b];
                }

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

                if(!ctx->ihdr.interlace_method)
                {
                    pixel_offset = (k + (scanline_idx * ctx->ihdr.width)) * pixel_size;
                }
                else
                {
                    const unsigned int adam7_x_start[7] = { 0, 4, 0, 2, 0, 1, 0 };
                    const unsigned int adam7_y_start[7] = { 0, 0, 4, 0, 2, 0, 1 };
                    const unsigned int adam7_x_delta[7] = { 8, 8, 4, 4, 2, 2, 1 };
                    const unsigned int adam7_y_delta[7] = { 8, 8, 8, 4, 4, 2, 2 };

                    pixel_offset = ((adam7_y_start[pass] + scanline_idx * adam7_y_delta[pass]) *
                                     ctx->ihdr.width + adam7_x_start[pass] + k * adam7_x_delta[pass]) * pixel_size;
                }

                memcpy((char*)out + pixel_offset, pixel, pixel_size);

            }/* for(k=0; k < sub[pass].width; k++) */

            scanline--; /* point to filter byte */

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
    spng__free(ctx, scanline_orig);
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
