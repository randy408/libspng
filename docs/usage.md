# Basic usage

libspng can decode images to an explicit output format (e.g. 8/16-bit RGBA)
from any PNG format. With `SPNG_FMT_PNG` the pixel format will identical
to the PNG's format in host-endian, or big-endian with `SPNG_FMT_RAW`,
the latter does not support any transforms e.g. gamma correction.


```c
#include <spng.h>

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

For a complete example see [example.c](https://github.com/randy408/libspng/blob/v0.7.3/examples/example.c)
and [Decoding untrusted files](decode.md#decoding-untrusted-files)
