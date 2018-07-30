# Overview

libspng is a C library for reading and writing Portable Network Graphics (PNG) 
format files with a focus on security and ease of use.

libspng is an alternative to libpng, the projects are separate and the APIs are
not compatible.

# Testing

Over 150 test images of different bit depth, color type, and size combinations 
are verified to decode to the same output as libpng.

# Safety

libspng does verify input data, it should be free of arithmetic overflow and buffer 
overflow errors.

# Usage

spng can decode images to 8- or 16-bit RGBA formats from any PNG file, whether to use
ancillary chunk information when decoding is controlled with `SPNG_DECODE_USE_*` flags,
by default they're ignored.

```c
/* Create a context */
struct spng_ctx *ctx = spng_ctx_new(0);

/* Set an input buffer */
spng_set_png_buffer(ctx, buf, buf_size);

/* Determine output image size */
spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &out_size);

/* Get an 8-bit RGBA image, regardless of PNG format */
spng_decode_image(ctx, SPNG_FMT_RGBA8, out, out_size, 0);

/* Get 16-bit RGBA image, do gamma-correction using gAMA chunk information if available.*/
spng_decode_image(ctx, out, out_size, SPNG_FMT_RGBA16, SPNG_DECODE_USE_GAMA);

/* Free context memory */
spng_ctx_free(ctx);
```

See [example.c](https://gitlab.com/randy408/libspng/blob/master/example.c).


# Build instructions

Building requires meson and ninja, zlib is a dependency and the testsuite also 
requires libpng.

## Building
```
meson build
cd build
ninja
```

## Running the testsuite
```
ninja test
```

## Benchmark

```
git clone --depth=1 https://gitlab.com/randy408/spng_images.git benchmark_images
cd build
meson configure -Dbuildtype=release
ninja benchmark
```
