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

    SPNG_FMT_GA8 = 16,
    SPNG_FMT_GA16 = 32,
    SPNG_FMT_G8 = 64,

    /* No conversion or scaling */
    SPNG_FMT_PNG = 256, /* host-endian */
    SPNG_FMT_RAW = 512  /* big-endian */
};
```

The channels are always in [byte-order](https://en.wikipedia.org/wiki/RGBA_color_model#RGBA8888) representation.

# spng_decode_flags

```c
enum spng_decode_flags
{
    SPNG_DECODE_USE_TRNS = 1, /* Deprecated */
    SPNG_DECODE_USE_GAMA = 2, /* Deprecated */

    SPNG_DECODE_TRNS = 1, /* Apply transparency */
    SPNG_DECODE_GAMMA = 2, /* Apply gamma correction */
    SPNG_DECODE_PROGRESSIVE = 256 /* Initialize for progressive reads */
};
```

# Supported format, flag combinations

| PNG Format   | Output format     | Flags  | Notes                                         |
|--------------|-------------------|--------|-----------------------------------------------|
| Any format*  | `SPNG_FMT_RGBA8`  | All    | Convert from any PNG format and bit depth     |
| Any format   | `SPNG_FMT_RGBA16` | All    | Convert from any PNG format and bit depth     |
| Any format   | `SPNG_FMT_RGB8`   | All    | Convert from any PNG format and bit depth     |
| Gray <=8-bit | `SPNG_FMT_G8`     | None** | Only valid for 1, 2, 4, 8-bit grayscale PNG's |
| Gray 16-bit  | `SPNG_FMT_GA16`   | All**  | Only valid for 16-bit grayscale PNG's         |
| Gray <=8-bit | `SPNG_FMT_GA8`    | All**  | Only valid for 1, 2, 4, 8-bit grayscale PNG's |
| Any format   | `SPNG_FMT_PNG`    | None** | The PNG's format in host-endian               |
| Any format   | `SPNG_FMT_RAW`    | None   | The PNG's format in big-endian                |


\* Any combination of color type and bit depth defined in the [standard](https://www.w3.org/TR/2003/REC-PNG-20031110/#table111).

\*\* Gamma correction is not implemented

The `SPNG_DECODE_PROGRESSIVE` flag is supported in all cases.

# Error handling

Decoding errors are divided into critical and non-critical errors.

See also: [General error handling](errors.md)

Critical errors are not recoverable, it should be assumed that decoding has
failed completely and any partial image output is invalid, although corrupted
PNG's may appear to decode to the same partial image every time this cannot be guaranteed.

A critical error will stop any further parsing, invalidate the context and return the
relevant error code, most functions check for a valid context state and return
`SPNG_EBADSTATE` for subsequent calls to prevent undefined behavior.
It is strongly recommended to check all return values.

Non-critical errors in a decoding context refers to file corruption issues
that can be handled in a deterministic manner either by ignoring checksums
or discarding invalid chunks.
The image is extracted consistently but may have lost color accuracy,
transparency, etc.

The default behavior is meant to emulate libpng for compatibility reasons
with existing images in the wild, most non-critical errors are ignored.
Configuring decoder strictness is currently limited to checksums.

* Invalid palette indices are handled as black, opaque pixels
* `tEXt`, `zTXt` chunks with non-Latin characters are considered valid
* Non-critical chunks are discarded if the:
    * Chunk CRC is invalid (`SPNG_CRC_DISCARD` is the default for ancillary chunks)
    * Chunk has an invalid DEFLATE stream, by default this includes Adler-32 checksum errors
    * Chunk has errors specific to the chunk type: unexpected length, out-of-range values, etc
* Critical chunks with either chunk CRC or Adler-32 errors will stop parsing (unless configured otherwise)
* Extra trailing image data is silently discarded
* No parsing or validation is done past the `IEND` end-of-file marker

Truncated PNG's and truncated image data is always handled as a critical error,
getting a partial image is possible with progressive decoding but is not
guaranteed to work in all cases. The decoder issues read callbacks that
can span multiple rows or even the whole image, partial reads are not processed.

Some limitations apply, spng will stop decoding if:

* An image row is larger than 4 GB
* Something causes arithmetic overflow (limited to extreme cases on 32-bit)

## Checksums

There are two types of checksums used in PNG's: 32-bit CRC's for chunk data and Adler-32
in DEFLATE streams.

Creating a context with the `SPNG_CTX_IGNORE_ADLER32` flag will cause Adler-32
checksums to be ignored by zlib, both in compressed metadata and image data.
Note this is only supported with zlib >= 1.2.11 and is not available when compiled against miniz.

Chunk CRC handling is configured with `spng_set_crc_action()`,
when `SPNG_CRC_USE` is used for either chunk types the Adler-32 checksums in DEFLATE streams
will be also ignored.
When set for both chunk types it has the same effect as `SPNG_CTX_IGNORE_ADLER32`,
this does not apply vice-versa.

Currently there are no distinctions made between Adler-32 checksum- and other errors
in DEFLATE streams, they're all mapped to the `SPNG_EZLIB` error code.

The libpng equivalent of `spng_set_crc_action()` is `png_set_crc_action()`,
it implements a subset of its features:

| libpng                 | spng               | Notes                   |
|------------------------|--------------------|-------------------------|
| `PNG_CRC_ERROR_QUIT`   | `SPNG_CRC_ERROR`   | Will not abort on error |
| `PNG_CRC_WARN_DISCARD` | `SPNG_CRC_DISCARD` | No warning system       |
| `PNG_CRC_QUIET_USE`    | `SPNG_CRC_USE`     |                         |


The `SPNG_CTX_IGNORE_ADLER32` context flag has the same effect as `PNG_IGNORE_ADLER32`
used with `png_set_option()`.

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
initialized with `fmt` and `flags` for progressive decoding,
the values of `out`, `len` are ignored.

The `SPNG_DECODE_TNRS` flag is silently ignored if the PNG does not
contain a tRNS chunk or is not applicable for the color type.

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
