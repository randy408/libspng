.. _context:

Context
=======

Data types
----------

.. c:type:: spng_ctx

   Context handle.

.. note:: The context handle has no public members.

.. c:type:: spng_crc_action

.. code-block:: c

      enum spng_crc_action
      {
          SPNG_CRC_ERROR = 0, /* Default */
          SPNG_CRC_DISCARD = 1, /* Discard chunk, invalid for critical chunks */
          SPNG_CRC_USE = 2 /* Ignore and don't calculate checksum */
      };


API
---

.. c:function:: spng_ctx *spng_ctx_new(int flags)

    Creates a new context.

.. c:function:: void spng_ctx_free(spng_ctx *ctx)

    Releases context resources.

.. c:function:: int spng_set_image_limits(spng_ctx *ctx, uint32_t width, uint32_t height)

    Set image width and height limits, these may not be larger than 2^31-1.

.. c:function:: int spng_get_image_limits(spng_ctx *ctx, uint32_t *width, uint32_t *height)

    Get image width and height limits.

    ``*width`` and ``*height`` must be non-NULL.

.. c:function:: int spng_set_crc_action(spng_ctx *ctx, int critical, int ancillary)

    Set how chunk CRC errors should be handled for critical and ancillary chunks.

.. note:: Partially implemented, ``SPNG_CRC_DISCARD`` has no effect.