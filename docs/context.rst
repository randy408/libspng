.. _context:

Context
=======

The context struct holds all state used for decoding and encoding.


Data types
----------

.. c:type:: struct spng_ctx

Context structure.


Public members
^^^^^^^^^^^^^^

The context structure has no public members.


API
---

.. c:function:: struct spng_ctx *spng_ctx_new(int flags)

Creates a new context.

.. c:function:: void spng_ctx_free(struct spng_ctx *ctx)

Releases context resources.

.. c:function:: int spng_set_image_limits(struct spng_ctx *ctx, uint32_t width, uint32_t height)

    Set image width and height limits, these may not be larger than 2^31-1.

.. c:function:: int spng_get_image_limits(struct spng_ctx *ctx, uint32_t *width, uint32_t *height)

    Get image width and height limits.

    ``*width`` and ``*height`` must be non-NULL.