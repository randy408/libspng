/* SPDX-License-Identifier: (BSD-2-Clause AND libpng-2.0) */

/* Include the implementation to test private helpers without exporting them. */
#include "../spng/spng.c"

#define CHECK(expr) do { if(!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while(0)

static int test_helpers(void)
{
    const struct
    {
        size_t a, b, sum, product;
        int add_overflow, mul_overflow;
    } cases[] =
    {
        {0, 0, 0, 0, 0, 0},
        {0, SIZE_MAX, SIZE_MAX, 0, 0, 0},
        {SIZE_MAX, 0, SIZE_MAX, 0, 0, 0},
        {1, SIZE_MAX, 0, SIZE_MAX, 1, 0},
        {SIZE_MAX, 1, 0, SIZE_MAX, 1, 0},
        {SIZE_MAX - 1, 1, SIZE_MAX, SIZE_MAX - 1, 0, 0},
        {SIZE_MAX, SIZE_MAX, 0, 0, 1, 1},
        {SIZE_MAX / 2, 2, SIZE_MAX / 2 + 2, SIZE_MAX - 1, 0, 0},
        {SIZE_MAX / 2 + 1, 2, SIZE_MAX / 2 + 3, 0, 0, 1},
        {2, SIZE_MAX / 2 + 1, SIZE_MAX / 2 + 3, 0, 0, 1},
        {SIZE_MAX - 14, 15, 0, 0, 1, 1},
        {SIZE_MAX - 15, 15, SIZE_MAX, 0, 0, 1},
        {SIZE_MAX - 31, 32, 0, 0, 1, 1},
        {SIZE_MAX - 32, 32, SIZE_MAX, 0, 0, 1},
        {17, 23, 40, 391, 0, 0}
    };

    size_t i;
    for(i=0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        size_t result = 42;
        CHECK(spng__ckd_add(&result, cases[i].a, cases[i].b) == cases[i].add_overflow);
        CHECK(result == (cases[i].add_overflow ? 42 : cases[i].sum));
        result = 42;
        CHECK(spng__ckd_mul(&result, cases[i].a, cases[i].b) == cases[i].mul_overflow);
        CHECK(result == (cases[i].mul_overflow ? 42 : cases[i].product));

        result = cases[i].a;
        CHECK(spng__ckd_add(&result, result, cases[i].b) == cases[i].add_overflow);
        CHECK(result == (cases[i].add_overflow ? cases[i].a : cases[i].sum));
        result = cases[i].a;
        CHECK(spng__ckd_mul(&result, result, cases[i].b) == cases[i].mul_overflow);
        CHECK(result == (cases[i].mul_overflow ? cases[i].a : cases[i].product));
    }

    return 0;
}

static unsigned allocation_calls;
static size_t allocation_size;

static void *record_malloc(size_t size)
{
    allocation_calls++;
    allocation_size = size;
    return NULL; /* Record the request without allocating it. */
}

static void *record_realloc(void *ptr, size_t size)
{
    (void)ptr;
    return record_malloc(size);
}

static int test_allocations(void)
{
    spng_ctx ctx = {0};
    ctx.alloc.malloc_fn = record_malloc;
    ctx.alloc.realloc_fn = record_realloc;

    allocation_calls = 0;
    CHECK(spng__zalloc(&ctx, 0, UINT_MAX) == NULL);
    CHECK(allocation_calls == 1 && allocation_size == 0);
    CHECK(spng__zalloc(&ctx, UINT_MAX, 0) == NULL);
    CHECK(allocation_calls == 2 && allocation_size == 0);
    CHECK(spng__zalloc(&ctx, 7, 9) == NULL);
    CHECK(allocation_calls == 3 && allocation_size == 63);
    allocation_calls = 0;
    CHECK(spng__zalloc(&ctx, UINT_MAX, UINT_MAX) == NULL);
    if((size_t)UINT_MAX > SIZE_MAX / UINT_MAX) CHECK(allocation_calls == 0);
    else CHECK(allocation_calls == 1 && allocation_size == (size_t)UINT_MAX * UINT_MAX);

#ifdef SPNG_USE_MINIZ
    /* miniz passes size_t operands, unlike zlib's uInt operands. */
    allocation_calls = 0;
    CHECK(spng__zalloc(&ctx, SIZE_MAX, 2) == NULL);
    CHECK(allocation_calls == 0);
    CHECK(spng__zalloc(&ctx, SIZE_MAX, 1) == NULL);
    CHECK(allocation_calls == 1 && allocation_size == SIZE_MAX);
    CHECK(spng__zalloc(&ctx, SIZE_MAX, 0) == NULL);
    CHECK(allocation_calls == 2 && allocation_size == 0);
#endif

    ctx.internal_buffer = 1;
    ctx.state = SPNG_STATE_OUTPUT;
    ctx.bytes_encoded = SIZE_MAX;
    allocation_calls = 0;
    CHECK(require_bytes(&ctx, 1) == SPNG_EOVERFLOW);
    CHECK(allocation_calls == 0 && ctx.state == SPNG_STATE_OUTPUT);

    ctx.out_png_size = SIZE_MAX / 2 + 1;
    ctx.bytes_encoded = ctx.out_png_size;
    CHECK(require_bytes(&ctx, 1) == SPNG_EOVERFLOW);
    CHECK(allocation_calls == 0 && ctx.state == SPNG_STATE_INVALID);

    ctx.state = SPNG_STATE_OUTPUT;
    ctx.out_png_size = SIZE_MAX / 2;
    ctx.bytes_encoded = ctx.out_png_size;
    CHECK(require_bytes(&ctx, 1) == SPNG_EMEM);
    CHECK(allocation_calls == 1 && allocation_size == SIZE_MAX - 1);

    ctx.state = SPNG_STATE_OUTPUT;
    ctx.out_png_size = 0;
    ctx.bytes_encoded = 0;
    CHECK(require_bytes(&ctx, SPNG_WRITE_SIZE * 2 + 1) == SPNG_EMEM);
    CHECK(allocation_calls == 2 && allocation_size == SPNG_WRITE_SIZE * 4);

    return 0;
}

static int test_image_sizes(void)
{
    struct spng_ihdr ihdr = {0};
    ihdr.width = 17;
    ihdr.height = 23;
    ihdr.bit_depth = 8;
    ihdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;
    size_t len = 0;

    CHECK(calculate_scanline_width(&ihdr, 17, &len) == 0 && len == 69);
    CHECK(calculate_image_size(&ihdr, SPNG_FMT_RGBA8, &len) == 0 && len == 1564);
    CHECK(calculate_image_size(&ihdr, SPNG_FMT_RGBA16, &len) == 0 && len == 3128);
    CHECK(calculate_image_size(&ihdr, SPNG_FMT_RGB8, &len) == 0 && len == 1173);
    CHECK(calculate_image_size(&ihdr, SPNG_FMT_PNG, &len) == 0 && len == 1564);
    CHECK(calculate_image_size(&ihdr, SPNG_FMT_RAW, &len) == 0 && len == 1564);

    ihdr.color_type = SPNG_COLOR_TYPE_GRAYSCALE;
    ihdr.bit_depth = 1;
    CHECK(calculate_scanline_width(&ihdr, 17, &len) == 0 && len == 4);
    CHECK(calculate_image_size(&ihdr, SPNG_FMT_PNG, &len) == 0 && len == 69);
    CHECK(calculate_image_size(&ihdr, SPNG_FMT_G8, &len) == 0 && len == 391);
    CHECK(calculate_image_size(&ihdr, SPNG_FMT_GA8, &len) == 0 && len == 782);
    ihdr.bit_depth = 16;
    CHECK(calculate_image_size(&ihdr, SPNG_FMT_GA16, &len) == 0 && len == 1564);
    CHECK(calculate_scanline_width(&ihdr, 0, &len) == SPNG_EINTERNAL);

    /* Test numeric limits only: no image or scanline allocation occurs. */
    ihdr.bit_depth = 8;
    len = 42;
    if(SIZE_MAX > UINT32_MAX)
    {
        CHECK(calculate_scanline_width(&ihdr, UINT32_MAX - 1, &len) == 0 && len == UINT32_MAX);
        len = 42;
    }
    CHECK(calculate_scanline_width(&ihdr, UINT32_MAX, &len) == SPNG_EOVERFLOW);
    CHECK(len == 42);

    ihdr.width = spng_u32max;
    ihdr.height = spng_u32max;
    ihdr.bit_depth = 16;
    ihdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;
    CHECK(calculate_image_size(&ihdr, SPNG_FMT_RGBA16, &len) == SPNG_EOVERFLOW);
    CHECK(len == 42);

    return 0;
}

static int test_inflate_growth(void)
{
    unsigned char input[10000], compressed[11000];
    memset(input, 'A', sizeof(input));
    uLongf compressed_len = sizeof(compressed);
    CHECK(compress2(compressed, &compressed_len, input, sizeof(input), Z_DEFAULT_COMPRESSION) == Z_OK);

    spng_ctx *ctx = spng_ctx_new(0);
    CHECK(ctx != NULL);
    char *out = NULL;
    size_t len = 0;
    CHECK(spng__inflate_stream(ctx, &out, &len, 1, compressed, compressed_len) == 0);
    CHECK(len == sizeof(input) + 1);
    CHECK(!memcmp(out, input, sizeof(input)));
    free(out);
    spng_ctx_free(ctx);

    ctx = spng_ctx_new(0);
    CHECK(ctx != NULL);
    ctx->chunk_cache_limit = 10000;
    out = NULL;
    len = 42;
    CHECK(spng__inflate_stream(ctx, &out, &len, 1, compressed, compressed_len) == SPNG_ECHUNK_LIMITS);
    CHECK(out == NULL && len == 42);
    spng_ctx_free(ctx);

    return 0;
}

static int test_public_overflow(void)
{
    /* A header and one IDAT byte suffice to reach the size checks. */
    unsigned char png[42] = {0};
    memcpy(png, spng_signature, 8);
    write_u32(png + 8, 13);
    memcpy(png + 12, type_ihdr, 4);
    write_u32(png + 16, spng_u32max);
    write_u32(png + 20, spng_u32max);
    png[24] = 8;
    png[25] = SPNG_COLOR_TYPE_GRAYSCALE;
    write_u32(png + 29, crc32(crc32(0, NULL, 0), png + 12, 17));
    write_u32(png + 33, 1);
    memcpy(png + 37, type_idat, 4);

    struct spng_alloc alloc =
    {
        .malloc_fn = record_malloc,
        .realloc_fn = record_realloc,
        .calloc_fn = calloc,
        .free_fn = free
    };
    spng_ctx *ctx = spng_ctx_new2(&alloc, 0);
    CHECK(ctx != NULL);
    allocation_calls = 0;
    CHECK(spng_set_png_buffer(ctx, png, sizeof(png)) == 0);
    size_t len = 42;
    CHECK(spng_decoded_image_size(ctx, SPNG_FMT_RGBA16, &len) == SPNG_EOVERFLOW);
    CHECK(len == 42 && allocation_calls == 0);

    if(SIZE_MAX > UINT32_MAX)
    {
        /* The output width fits, but the whole image does not. Progressive
           decoding must retain the zero sentinel and proceed past it. */
        unsigned char out = 0;
        ctx->image_size = 42;
        CHECK(spng_decode_image(ctx, &out, 1, SPNG_FMT_RGBA16, 0) == SPNG_EOVERFLOW);
        CHECK(ctx->image_size == 0 && allocation_calls == 0);
        ctx->image_size = 42;
        CHECK(spng_decode_image(ctx, NULL, 0, SPNG_FMT_RGBA16, SPNG_DECODE_PROGRESSIVE) == SPNG_EZLIB_INIT);
        CHECK(ctx->image_size == 0 && allocation_calls == 1);
    }
    spng_ctx_free(ctx);

    if(SIZE_MAX == UINT32_MAX)
    {
        /* On 32-bit targets, a one-pixel row fits while its total image
           overflows. Exercise both progressive sentinels without allocation. */
        write_u32(png + 16, 1);
        png[25] = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;
        write_u32(png + 29, crc32(crc32(0, NULL, 0), png + 12, 17));
        ctx = spng_ctx_new2(&alloc, 0);
        CHECK(ctx != NULL);
        allocation_calls = 0;
        CHECK(spng_set_png_buffer(ctx, png, sizeof(png)) == 0);
        len = 42;
        CHECK(spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &len) == SPNG_EOVERFLOW);
        CHECK(len == 42 && allocation_calls == 0);
        unsigned char out = 0;
        ctx->image_size = 42;
        CHECK(spng_decode_image(ctx, &out, 1, SPNG_FMT_RGBA8, 0) == SPNG_EOVERFLOW);
        CHECK(ctx->image_size == 0 && allocation_calls == 0);
        ctx->image_size = 42;
        CHECK(spng_decode_image(ctx, NULL, 0, SPNG_FMT_RGBA8, SPNG_DECODE_PROGRESSIVE) == SPNG_EZLIB_INIT);
        CHECK(ctx->image_size == 0 && allocation_calls == 1);
        spng_ctx_free(ctx);

        ctx = spng_ctx_new2(&alloc, SPNG_CTX_ENCODER);
        CHECK(ctx != NULL);
        struct spng_ihdr narrow = {0};
        narrow.width = 1;
        narrow.height = spng_u32max;
        narrow.bit_depth = 8;
        narrow.color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;
        CHECK(spng_set_ihdr(ctx, &narrow) == 0);
        CHECK(spng_set_option(ctx, SPNG_ENCODE_TO_BUFFER, 1) == 0);
        allocation_calls = 0;
        ctx->image_size = 42;
        CHECK(spng_encode_image(ctx, &out, 1, SPNG_FMT_RAW, 0) == SPNG_EOVERFLOW);
        CHECK(ctx->image_size == 0 && allocation_calls == 0);
        ctx->image_size = 42;
        CHECK(spng_encode_image(ctx, NULL, 0, SPNG_FMT_RAW, SPNG_ENCODE_PROGRESSIVE) == SPNG_EMEM);
        CHECK(ctx->image_size == 0 && allocation_calls == 1);
        spng_ctx_free(ctx);
    }

    ctx = spng_ctx_new2(&alloc, SPNG_CTX_ENCODER);
    CHECK(ctx != NULL);
    struct spng_ihdr ihdr = {0};
    ihdr.width = spng_u32max;
    ihdr.height = 1;
    ihdr.bit_depth = 16;
    ihdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;
    CHECK(spng_set_ihdr(ctx, &ihdr) == 0);
    allocation_calls = 0;
    unsigned char input = 0;
    CHECK(spng_encode_image(ctx, &input, 1, SPNG_FMT_PNG, 0) == SPNG_EOVERFLOW);
    CHECK(allocation_calls == 0);
    spng_ctx_free(ctx);

    return 0;
}

static int test_roundtrip(void)
{
    const unsigned char pixels[24] =
    {
        0, 1, 2, 255, 11, 22, 33, 128, 254, 253, 252, 0,
        10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120
    };
    unsigned interlace;
    for(interlace=0; interlace < 2; interlace++)
    {
        struct spng_ihdr ihdr = {0};
        ihdr.width = 3;
        ihdr.height = 2;
        ihdr.bit_depth = 8;
        ihdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;
        ihdr.interlace_method = interlace;

        spng_ctx *encoder = spng_ctx_new(SPNG_CTX_ENCODER);
        CHECK(encoder != NULL);
        struct spng_ihdr invalid = ihdr;
        invalid.width = 0;
        CHECK(spng_set_ihdr(encoder, &invalid) == SPNG_EWIDTH);
        invalid = ihdr;
        invalid.height = 0;
        CHECK(spng_set_ihdr(encoder, &invalid) == SPNG_EHEIGHT);
        CHECK(spng_set_ihdr(encoder, &ihdr) == 0);
        CHECK(spng_set_option(encoder, SPNG_ENCODE_TO_BUFFER, 1) == 0);
        CHECK(spng_encode_image(encoder, NULL, sizeof(pixels), SPNG_FMT_PNG, 0) == 1);
        CHECK(spng_encode_image(encoder, pixels, sizeof(pixels) - 1, SPNG_FMT_PNG, 0) == SPNG_EBUFSIZ);
        CHECK(spng_encode_image(encoder, pixels, sizeof(pixels), SPNG_FMT_PNG, SPNG_ENCODE_FINALIZE) == 0);

        size_t png_len = 0;
        int error = -1;
        void *png = spng_get_png_buffer(encoder, &png_len, &error);
        CHECK(png != NULL && error == 0);
        spng_ctx_free(encoder);

        spng_ctx *decoder = spng_ctx_new(0);
        CHECK(decoder != NULL);
        CHECK(spng_set_png_buffer(decoder, png, png_len) == 0);
        size_t len = 0;
        CHECK(spng_decoded_image_size(decoder, SPNG_FMT_RGBA8, &len) == 0 && len == sizeof(pixels));
        unsigned char decoded[sizeof(pixels)];
        CHECK(spng_decode_image(decoder, NULL, len, SPNG_FMT_RGBA8, 0) == 1);
        CHECK(spng_decode_image(decoder, decoded, len - 1, SPNG_FMT_RGBA8, 0) == SPNG_EBUFSIZ);
        CHECK(spng_decode_image(decoder, decoded, len, SPNG_FMT_RGBA8, 0) == 0);
        CHECK(!memcmp(decoded, pixels, sizeof(pixels)));
        spng_ctx_free(decoder);
        free(png);
    }

    return 0;
}

static int test_roundtrip_matrix(void)
{
    const struct { unsigned color, depth; } formats[] =
    {
        {SPNG_COLOR_TYPE_GRAYSCALE, 1}, {SPNG_COLOR_TYPE_GRAYSCALE, 2},
        {SPNG_COLOR_TYPE_GRAYSCALE, 4}, {SPNG_COLOR_TYPE_GRAYSCALE, 8},
        {SPNG_COLOR_TYPE_GRAYSCALE, 16}, {SPNG_COLOR_TYPE_TRUECOLOR, 8},
        {SPNG_COLOR_TYPE_TRUECOLOR, 16}, {SPNG_COLOR_TYPE_INDEXED, 1},
        {SPNG_COLOR_TYPE_INDEXED, 2}, {SPNG_COLOR_TYPE_INDEXED, 4},
        {SPNG_COLOR_TYPE_INDEXED, 8}, {SPNG_COLOR_TYPE_GRAYSCALE_ALPHA, 8},
        {SPNG_COLOR_TYPE_GRAYSCALE_ALPHA, 16}, {SPNG_COLOR_TYPE_TRUECOLOR_ALPHA, 8},
        {SPNG_COLOR_TYPE_TRUECOLOR_ALPHA, 16}
    };
    const struct { uint32_t width, height; } dimensions[] =
    {
        {1, 1}, {7, 3}, {8, 4}, {9, 5}, {17, 7}, {257, 33}
    };
    unsigned f, d, interlace, progressive, decode_progressive;
    unsigned grew_buffer = 0;
    uint32_t random = 217;

    for(f=0; f < sizeof(formats) / sizeof(formats[0]); f++)
    for(d=0; d < sizeof(dimensions) / sizeof(dimensions[0]); d++)
    for(interlace=0; interlace < 2; interlace++)
    for(progressive=0; progressive < 2; progressive++)
    {
        struct spng_ihdr ihdr = {0};
        ihdr.width = dimensions[d].width;
        ihdr.height = dimensions[d].height;
        ihdr.bit_depth = formats[f].depth;
        ihdr.color_type = formats[f].color;
        ihdr.interlace_method = interlace;
        size_t row_bits = ihdr.width * num_channels(&ihdr) * ihdr.bit_depth;
        size_t row_size = (row_bits + 7) / 8;
        size_t image_size = row_size * ihdr.height;
        unsigned char *pixels = malloc(image_size), *decoded = malloc(image_size);
        CHECK(pixels != NULL && decoded != NULL);
        size_t i;
        for(i=0; i < image_size; i++)
        {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            pixels[i] = random;
        }
        if(row_bits % 8)
        {
            for(i=0; i < ihdr.height; i++) pixels[(i + 1) * row_size - 1] &= 255 << (8 - row_bits % 8);
        }

        spng_ctx *encoder = spng_ctx_new(SPNG_CTX_ENCODER);
        CHECK(encoder != NULL);
        CHECK(spng_set_ihdr(encoder, &ihdr) == 0);
        if(ihdr.color_type == SPNG_COLOR_TYPE_INDEXED)
        {
            struct spng_plte plte = {0};
            plte.n_entries = 1 << ihdr.bit_depth;
            for(i=0; i < plte.n_entries; i++)
            {
                plte.entries[i].red = i;
                plte.entries[i].green = 255 - i;
                plte.entries[i].blue = i / 2;
                plte.entries[i].alpha = 255;
            }
            CHECK(spng_set_plte(encoder, &plte) == 0);
        }
        CHECK(spng_set_option(encoder, SPNG_ENCODE_TO_BUFFER, 1) == 0);
        int ret;
        if(progressive)
        {
            CHECK(spng_encode_image(encoder, NULL, 0, SPNG_FMT_RAW, SPNG_ENCODE_PROGRESSIVE | SPNG_ENCODE_FINALIZE) == 0);
            do
            {
                struct spng_row_info info;
                CHECK(spng_get_row_info(encoder, &info) == 0);
                ret = spng_encode_row(encoder, pixels + info.row_num * row_size, row_size);
            }while(!ret);
            CHECK(ret == SPNG_EOI);
        }
        else CHECK(spng_encode_image(encoder, pixels, image_size, SPNG_FMT_RAW, SPNG_ENCODE_FINALIZE) == 0);
        size_t png_size;
        int error;
        void *png = spng_get_png_buffer(encoder, &png_size, &error);
        CHECK(png != NULL && error == 0);
        if(png_size > SPNG_WRITE_SIZE * 2) grew_buffer = 1;
        spng_ctx_free(encoder);

        for(decode_progressive=0; decode_progressive < 2; decode_progressive++)
        {
            spng_ctx *decoder = spng_ctx_new(0);
            CHECK(decoder != NULL);
            CHECK(spng_set_png_buffer(decoder, png, png_size) == 0);
            size_t decoded_size = 0;
            CHECK(spng_decoded_image_size(decoder, SPNG_FMT_RAW, &decoded_size) == 0 && decoded_size == image_size);
            memset(decoded, 0, image_size);
            if(decode_progressive)
            {
                CHECK(spng_decode_image(decoder, NULL, 0, SPNG_FMT_RAW, SPNG_DECODE_PROGRESSIVE) == 0);
                do
                {
                    struct spng_row_info info;
                    ret = spng_get_row_info(decoder, &info);
                    if(ret) break;
                    ret = spng_decode_row(decoder, decoded + info.row_num * row_size, row_size);
                }while(!ret);
                CHECK(ret == SPNG_EOI);
            }
            else CHECK(spng_decode_image(decoder, decoded, image_size, SPNG_FMT_RAW, 0) == 0);
            CHECK(!memcmp(decoded, pixels, image_size));
            CHECK(spng_decode_chunks(decoder) == 0);
            spng_ctx_free(decoder);
        }
        free(png);
        free(pixels);
        free(decoded);
    }

    CHECK(grew_buffer);
    return 0;
}

int main(void)
{
    if(test_helpers() || test_allocations() || test_image_sizes() ||
       test_inflate_growth() || test_public_overflow() || test_roundtrip() ||
       test_roundtrip_matrix()) return 1;

    return 0;
}
