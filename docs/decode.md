# Data types

```c
typedef int spng_read_fn(spng_ctx *ctx, void *user, void *dest, size_t length)
```

Type definition for callback passed to `spng_set_png_stream()`.

A read callback function should copy `length` bytes to `dest` and return 0 or
`SPNG_IO_EOF`/`SPNG_IO_ERROR` on error.

# spng_format

```c
enum spng_format
{
    SPNG_FMT_RGBA8 = 1,
    SPNG_FMT_RGBA16 = 2,
    SPNG_FMT_RGB8 = 4,
    SPNG_FMT_PNG = 16,
    SPNG_FMT_RAW = 32
};
```

The channels are always in [byte-order](https://en.wikipedia.org/wiki/RGBA_color_model#RGBA_(byte-order)) representation.

# spng_decode_flags

```c
enum spng_decode_flags
{
    SPNG_DECODE_USE_TRNS = 1, /* deprecated */
    SPNG_DECODE_USE_GAMA = 2, /* deprecated */

    SPNG_DECODE_TRNS = 1,
    SPNG_DECODE_GAMMA = 2
};
```

# Supported format, flag combinations

| PNG Format   | Output format     | Flags  | Notes                                       |
|--------------|-------------------|--------|---------------------------------------------|
| Any format*  | `SPNG_FMT_RGBA8`  | All    | Convert from any PNG format and bit depth   |
| Any format   | `SPNG_FMT_RGBA16` | All    | Convert from any PNG format and bit depth   |
| Any format   | `SPNG_FMT_RGB8`   | All    | Convert from any PNG format and bit depth   |
| Any format   | `SPNG_FMT_PNG`    | None** | The PNG's format in host-endian             |
| Any format   | `SPNG_FMT_RAW`    | None   | The PNG's format in big-endian              |
| Gray <=8-bit | `SPNG_FMT_G8`     | None** | Valid for 1, 2, 4, 8-bit grayscale only     |

\* Any combination of color type and bit depth defined in the [standard](https://www.w3.org/TR/2003/REC-PNG-20031110/#table111).

\*\* Gamma correction is not implemented

# API

# spng_set_png_buffer()
```c
int spng_set_png_buffer(spng_ctx *ctx, void *buf, size_t size)
```

Set input PNG buffer, this can only be done once per context.

# spng_set_png_stream()
```c
int spng_set_png_stream(spng_ctx *ctx, spng_read_fn *read_fn, void *user)
```

Set input PNG stream, this can only be done once per context.

!!! info
    PNG's are read up to the file end marker, this is identical behavior to libpng.

# spng_set_png_file()
```c
int spng_set_png_file(spng_ctx *ctx, FILE *file)
```

Set input PNG file, this can only be done once per context.

# spng_decoded_image_size()
```c
int spng_decoded_image_size(spng_ctx *ctx, int fmt, size_t *out)
```

Calculates decoded image buffer size for the given output format.

An input PNG must be set.

# spng_decode_image()
```c
int spng_decode_image(spng_ctx *ctx, void *out, size_t len, int fmt, int flags)
```

Decodes the PNG file and writes the image to `out`,
the image is converted from any PNG format to the destination format `fmt`.

Interlaced images are deinterlaced, 16-bit images are converted to host-endian.

`out` must point to a buffer of length `len`.

`len` must be equal to or greater than the number calculated with
`spng_decoded_image_size()` with the same output format.

If the `SPNG_DECODE_PROGRESSIVE` flag is set the decoder will be
initialized with `fmt`, `flags` for progressive decoding,
the image is not decoded and the values of `out`, `len` are ignored.

The `SPNG_DECODE_TNRS` flag is ignored if the PNG has an alpha channel
or does not contain a tRNS chunk, it is also ignored for gray 1/2/4-bit images.

This function can only be called once per context.

# spng_decode_scanline()
```c
int spng_decode_scanline(spng_ctx *ctx, void *out, size_t len)
```

Decodes a scanline to `out`.

This function requires the decoder to be initialized by calling
`spng_decode_image()` with the `SPNG_DECODE_PROGRESSIVE` flag set.

The widest scanline is the decoded image size divided by `ihdr.height`.

For the last scanline and subsequent calls the return value is `SPNG_EOI`.

# spng_decode_row()
```c
int spng_decode_row(spng_ctx *ctx, void *out, size_t len)
```

Decodes and deinterlaces a scanline to `out`.

This function requires the decoder to be initialized by calling
`spng_decode_image()` with the `SPNG_DECODE_PROGRESSIVE` flag set.

The width of the row is the decoded image size divided by `ihdr.height`.

For the last row and subsequent calls the return value is `SPNG_EOI`.

If the image is not interlaced this function's behavior is identical to
`spng_decode_scanline()`.

# spng_get_row_info()
```c
int spng_get_row_info(spng_ctx *ctx, struct spng_row_info *row_info)
```

Copies the current, to-be-decoded row's information to `row_info`.
