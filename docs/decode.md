# Data types

```c
typedef int spng_read_fn(spng_ctx *ctx, void *user, void *dest, size_t length)
```

Type definition for callback passed to `spng_set_png_stream`.

A read callback function should copy `length` bytes to `dest` and return 0 or
`SPNG_IO_EOF`/`SPNG_IO_ERROR` on error.

# spng_format

```c
enum spng_format
{
    SPNG_FMT_RAW = 0,
    SPNG_FMT_RGBA8 = 1,
    SPNG_FMT_RGBA16 = 2
};
```
The `RGBA8` and `RGBA16` formats convert images of all color types to
full RGBA format, scaling the bit depth if needed. Indexed color images
are expanded by applying the look up table defined in the PLTE chunk.
Additionally, images are deinterlaced if needed and 16 bit images are
byte-swapped to host endianness.

The `RAW` format will deinterlace the image but otherwise leaves the
data untransformed. 1, 2 and 4 bit depth data will remain in byte packed
rows, chunks that modify image data (PLTE, tRANS, gAMA, etc.) are not
applied and 16bit data is left in big endian byte order.

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


# API

# spng_set_png_buffer()
```c
int spng_set_png_buffer(spng_ctx *ctx, void *buf, size_t size)
```

Set PNG buffer, this can only be called once per context.

# spng_set_png_stream()
```c
int spng_set_png_stream(spng_ctx *ctx, spng_read_fn *read_fn, void *user)
```

Set PNG stream, this can only be called once per context.

!!! info
    PNG's are read up to the file end marker, this is identical behavior to libpng.

# spng_decoded_image_size()
```c
int spng_decoded_image_size(spng_ctx *ctx, int fmt, size_t *out)
```

Calculates decoded image buffer size for the given output format.

An input PNG must be set.

# spng_decode_image()
```c
int spng_decode_image(spng_ctx *ctx, void *out, size_t out_size, int fmt, int flags)`
```

Decodes the PNG file and writes the decoded image to `out`.

`out` must point to a buffer of length `out_size`.

`out_size` must be equal to or greater than the number calculated with
`spng_decoded_image_size` with the same output format.

Interlaced images are written deinterlaced to `out`,
16-bit images are converted to host-endian (unless fmt == `SPNG_FMT_RAW`).

This function can only be called once per context.
