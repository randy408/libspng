# Data types

# spng_encode_flags

```c
enum spng_encode_flags
{
    SPNG_ENCODE_PROGRESSIVE = 1, /* Initialize for progressive writes */
    SPNG_ENCODE_FINALIZE = 2, /* Finalize PNG after encoding image */
};
```

## Memory usage

On top of the overhead of the the context buffer the internal write buffer
may grow to the length of entire chunks or more than the length of
the PNG file when no output stream or file is set.

Encoding an image requires at least 2 rows to be kept in memory,
this may increase to 3 rows for future versions.

# API

See also: [spng_set_png_stream()](context.md#spng_set_png_stream), [spng_set_png_file()](context.md#spng_set_png_file).

# spng_encode_chunks()
```c
int spng_encode_chunks(spng_ctx *ctx)
```

Encode all stored chunks before or after the image data (IDAT) stream,
depending on the state of the encoder.

If the image is encoded this function will also finalize the PNG with the end-of-file (IEND) marker.

Calling this function before `spng_encode_image()` is optional.

# spng_encode_image()
```c
int spng_encode_image(spng_ctx *ctx, const void *img, size_t len, int fmt, int flags)
```

Encodes the image from `img` in the source format `fmt` to the specified PNG format set with [spng_set_ihdr()](chunk.md#spng_set_ihdr).

The width, height, color type, bit depth and interlace method must be set with [spng_set_ihdr()](chunk.md#spng_set_ihdr).

Format conversion is currently not supported, the image format must match the
color type and bit depth set with [spng_set_ihdr()](chunk.md#spng_set_ihdr),
`fmt` must be `SPNG_FMT_PNG`.

`img` must point to a buffer of length `len`, `len` must be equal to the expected image
size for the given format `fmt`.

This function may call `spng_encode_chunks()`, writing any pending chunks before the image data.

If the `SPNG_ENCODE_FINALIZE` flag is set the encoder will any write any pending chunks
after the image data and finalize the PNG with the end-of-file (IEND) marker.
In most cases the only pending data is the end-of-file marker, which is 12 bytes.

If no output stream or file is set the encoder will allocate and write to an internal buffer,
[spng_get_png_buffer()](#spng_get_png_buffer) will return the encoded buffer,
this must be freed by the user. If this function isn't called or an error is encountered
the internal buffer is freed with [spng_ctx_free()](context.md#spng_ctx_free).

16-bit images are assumed to be host-endian except for `SPNG_FMT_RAW`.

## Progressive image encoding

If the `SPNG_ENCODE_PROGRESSIVE` flag is set the encoder will be initialized
with `fmt` and `flags` for progressive encoding, the values of `img`, `len` are ignored.

With the `SPNG_ENCODE_FINALIZE` flag set the PNG is finalized on the last scanline
or row.

Progressive encoding is straightforward when the image is not interlaced,
calling [spng_encode_row()](#spng_encode_row) for each row of the image will yield
the return value `SPNG_EOI` for the final row:

```c
int error;
size_t image_width = image_size / ihdr.height;

for(i = 0; i < ihdr.height; i++)
{
    void *row = image + image_width * i;

    error = spng_encode_row(ctx, row, image_width);

    if(error) break;
}

if(error == SPNG_EOI) /* success */
```

But for interlaced images rows are accessed multiple times and non-sequentially,
use [spng_get_row_info()](context.md#spng_get_row_info) to get the current row number:

```c
int error;
struct spng_row_info row_info;

do
{
    error = spng_get_row_info(ctx, &row_info);
    if(error) break;

    void *row = image + image_width * row_info.row_num;

    error = spng_encode_row(ctx, row, len);
}
while(!error)

if(error == SPNG_EOI) /* success */
```

# spng_encode_scanline()
```c
int spng_encode_scanline(spng_ctx *ctx, const void *scanline, size_t len)
```

Encodes `scanline`, this function is meant to be used for interlaced PNG's
with image data that is already split into multiple passes.

This function requires the encoder to be initialized by calling
`spng_encode_image()` with the `SPNG_ENCODE_PROGRESSIVE` flag set.

For the last scanline and subsequent calls the return value is `SPNG_EOI`.

# spng_encode_row()
```c
int spng_encode_row(spng_ctx *ctx, const void *row, size_t len)
```

Encodes `row`, interlacing it if necessary.

This function requires the encoder to be initialized by calling
`spng_encode_image()` with the `SPNG_ENCODE_PROGRESSIVE` flag set.

See also: [Progressive-image-encoding](#Progressive-image-encoding)

For the last row and subsequent calls the return value is `SPNG_EOI`.

If the image is not interlaced this function's behavior is identical to
`spng_encode_scanline()`.


# spng_get_png_buffer()
```c
void *spng_get_png_buffer(spng_ctx *ctx, size_t *len, int *error)
```

If the context is an encoder and no output stream or file was set the encoded buffer is returned,
it must be called after [spng_encode_image()](encode.md#spng_encode_image),
the PNG must be finalized.

On success the buffer must be freed by the user.
