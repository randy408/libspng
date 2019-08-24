# Data types

# spng_ctx
```c
typedef struct spng_ctx spng_ctx;
```

   Context handle.

!!!note
    The context handle has no public members.


# spng_ctx_flags

```c
enum spng_ctx_flags
{
    SPNG_CTX_IGNORE_ADLER32 = 1 /* ignore checksum in DEFLATE streams */
};
```

# spng_crc_action

```c
enum spng_crc_action
{
    SPNG_CRC_ERROR = 0, /* Default */
    SPNG_CRC_DISCARD = 1, /* Discard chunk, invalid for critical chunks */
    SPNG_CRC_USE = 2 /* Ignore and don't calculate checksum */
};
```

# API

# spng_ctx_new()
```c
spng_ctx *spng_ctx_new(int flags)`
```

Creates a new context.

# spng_ctx_new2()
```c
spng_ctx *spng_ctx_new2(struct spng_alloc *alloc, int flags)
```

Creates a new context with a custom memory allocator, `alloc` must be non-NULL.

# spng_ctx_free()
```c
void spng_ctx_free(spng_ctx *ctx)
```

Releases context resources.

# spng_set_image_limits()
```c
int spng_set_image_limits(spng_ctx *ctx, uint32_t width, uint32_t height)
```

Set image width and height limits, these may not be larger than 2<sup>31</sup>-1.

# spng_get_image_limits()
```c
int spng_get_image_limits(spng_ctx *ctx, uint32_t *width, uint32_t *height)
```

Get image width and height limits.

`width` and `height` must be non-NULL.

# spng_set_crc_action()
```c
int spng_set_crc_action(spng_ctx *ctx, int critical, int ancillary)`
```

Set how chunk CRC errors should be handled for critical and ancillary chunks.

!!!note
    Partially implemented, `SPNG_CRC_DISCARD` has no effect.

# spng_set_chunk_limits()
```c
int spng_set_chunk_limits(spng_ctx *ctx, size_t chunk_size, size_t cache_limit)
```

Set chunk size and chunk cache limits, the default chunk size limit is 2<sup>31</sup>-1,
the default chunk cache limit is `SIZE_MAX`.

!!!note
    This can only be used for limiting memory usage, most standard chunks
    do not require additional memory and are stored regardless of these limits.

# spng_get_chunk_limits()
```c
int spng_get_chunk_limits(spng_ctx *ctx, size_t *chunk_size, size_t *cache_limit)
```

Get chunk size and chunk cache limits.
