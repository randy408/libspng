# Basic usage

libspng can decode images to 8- or 16-bit RGBA formats from any PNG file,
whether to use ancillary chunk information when decoding is controlled
with `SPNG_DECODE_USE_*` flags, they're ignored by default. To output the
image data untransformed (except for deinterlacing) use `SPNG_FMT_RAW`,
no flags can be used in "RAW" output format.


```c

/* Create a context */
spng_ctx *ctx = spng_ctx_new(0);

/* Set an input buffer */
spng_set_png_buffer(ctx, buf, buf_size);

/* Calculate output image size */
spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &out_size);

/* Get an 8-bit RGBA image regardless of PNG format */
spng_decode_image(ctx, SPNG_FMT_RGBA8, out, out_size, 0);

/* Free context memory */
spng_ctx_free(ctx);

```

For a complete example see [example.c](https://github.com/randy408/libspng/blob/v0.6.0/examples/example.c)


## Decoding arbitrary files

To decode arbitrary files safely it is required to at least:

* Set an upper image width and height limit with `spng_set_image_limits()`.

* Use `spng_decoded_image_size()` to calculate the output image size
 and check it against a constant limit.

* Set a chunk size and chunk cache limit with `spng_set_chunks_limits()` to avoid
 running out of memory.

### Notes on memory use

The library allocates 2 to 3 times the width of the PNG image for decoding.

Gamma correction requires an additional 128KB for a lookup table if
the output format has 16-bits per channel (e.g. `SPNG_FMT_RGBA16`).
