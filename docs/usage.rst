.. _usage:

Usage
=====

Basic usage
-----------

libspng can decode images to 8- or 16-bit RGBA formats from any PNG file,
  whether to use ancillary chunk information when decoding is controlled
  with `SPNG_DECODE_USE_*` flags,
  by default they're ignored.

.. code-block:: c

    /* Create a context */
    spng_ctx *ctx = spng_ctx_new(0);

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


For a complete example see `example.c <https://gitlab.com/randy408/libspng/blob/v0.4.4/examples/example.c>`_.
