# Basic usage

libspng can decode images to 8- or 16-bit RGBA formats from any PNG file,
whether to use ancillary chunk information when decoding is controlled
with `SPNG_DECODE_USE_*` flags, they're ignored by default.

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


For a complete example see [example.c](https://github.com/randy408/libspng/blob/v0.5.0/examples/example.c)
