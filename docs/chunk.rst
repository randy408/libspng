.. _chunk:

Retrieving and setting chunks
=============================

Semantics
---------

* Chunk data is stored in :c:type:`spng_ctx`.
* When calling ``spng_get_*()`` or ``spng_set_*()`` functions all ancillary chunks
  up to the first IDAT are read, validated then stored.
* ``spng_set_*()`` functions replace stored chunk data.
* Chunk data stored with ``spng_set_*()`` functions are never replaced with input
  file chunk data i.e. if you set something it will stay that way.


Data types
----------

.. c:type:: struct spng_ihdr

    Image header information

.. code-block:: c

      enum spng_color_type
      {
          SPNG_COLOR_TYPE_GRAYSCALE = 0,
          SPNG_COLOR_TYPE_TRUECOLOR = 2,
          SPNG_COLOR_TYPE_INDEXED = 3,
          SPNG_COLOR_TYPE_GRAYSCALE_ALPHA = 4,
          SPNG_COLOR_TYPE_TRUECOLOR_ALPHA = 6
      };

      struct spng_ihdr
      {
          uint32_t width;
          uint32_t height;
          uint8_t bit_depth;
          uint8_t color_type;
          uint8_t compression_method;
          uint8_t filter_method;
          uint8_t interlace_method;
      };

.. c:type:: struct spng_plte

      Image palette

.. code-block:: c

          struct spng_plte_entry
          {
              uint8_t red;
              uint8_t green;
              uint8_t blue;
          };

          struct spng_plte
          {
              uint32_t n_entries;
              struct spng_plte_entry entries[256];
          };

.. c:type:: struct spng_trns

      Transparency

.. code-block:: c

          struct spng_trns_type2
          {
              uint16_t red;
              uint16_t green;
              uint16_t blue;
          };

          struct spng_trns
          {
              uint32_t n_type3_entries;
              union
              {
                  uint16_t type0_grey_sample;
                  struct spng_trns_type2 type2;
                  uint8_t type3_alpha[256];
              };
          };



.. c:type:: struct spng_chrm

      Image chromacities and white point

.. code-block:: c

        struct spng_chrm
        {
            double white_point_x;
            double white_point_y;
            double red_x;
            double red_y;
            double green_x;
            double green_y;
            double blue_x;
            double blue_y;
        };

.. c:type:: struct spng_chrm_int

      Image chromacities and white point in PNG's internal representation

        struct spng_chrm_int
        {
            uint32_t white_point_x;
            uint32_t white_point_y;
            uint32_t red_x;
            uint32_t red_y;
            uint32_t green_x;
            uint32_t green_y;
            uint32_t blue_x;
            uint32_t blue_y;
        };


.. c:type::struct spng_iccp

    Image ICC color profile

    ::

.. c:type:: struct spng_sbit

    Significant sample bits

.. code-block:: c

        struct spng_sbit
        {
            uint8_t greyscale_bits;
            uint8_t red_bits;
            uint8_t green_bits;
            uint8_t blue_bits;
            uint8_t alpha_bits;
        };

.. c:type:: struct spng_text

    Text information

.. code-block:: c

        enum spng_text_type
        {
            SPNG_TEXT = 1,
            SPNG_ZTXT = 2,
            SPNG_ITXT = 3
        };

        struct spng_text
        {
            char keyword[80];
            int type;

            size_t length;
            char *text;

            uint8_t compression_flag; /* iTXt only */
            uint8_t compression_method; /* iTXt, ztXt only */
            char *language_tag; /* iTXt only */
            char *translated_keyword; /* iTXt only */
        };


.. c:type:: struct spng_bkgd

    Image background color

.. code-block:: c

        struct spng_bkgd_type2_6
        {
            uint16_t red;
            uint16_t green;
            uint16_t blue;
        };

        struct spng_bkgd
        {
            union
            {
                uint16_t type0_4_greyscale;
                struct spng_bkgd_type2_6 type2_6;
                uint8_t type3_plte_index;
            };
        };

.. c:type:: struct spng_hist

    Image histogram

.. code-block:: c

        struct spng_hist
        {
            uint16_t frequency[256];
        };


.. c:type:: struct spng_phys

    Physical pixel dimensions

.. code-block:: c

        struct spng_phys
        {
            uint32_t ppu_x, ppu_y;
            uint8_t unit_specifier;
        };

.. c:type:: struct spng_splt

    Suggested palettes

.. code-block:: c

        struct spng_splt_entry
        {
            uint16_t red;
            uint16_t green;
            uint16_t blue;
            uint16_t alpha;
            uint16_t frequency;
        };

        struct spng_splt
        {
            char name[80];
            uint8_t sample_depth;
            uint32_t n_entries;
            struct spng_splt_entry *entries;
        };

.. c:type:: struct spng_time

    Image modification time

.. code-block:: c

        struct spng_time
        {
            uint16_t year;
            uint8_t month;
            uint8_t day;
            uint8_t hour;
            uint8_t minute;
            uint8_t second;
        };

.. c:type:: struct spng_offs

    Image offset

.. code-block:: c

        struct spng_offs
        {
            int32_t x, y;
            uint8_t unit_specifier;
        };


.. c:type:: struct spng_exif

    EXIF information

.. code-block:: c

        struct spng_exif
        {
            size_t length;
            char *data;
        };


API
---

.. c:function:: int spng_get_ihdr(struct spng_ctx *ctx, struct spng_ihdr *ihdr)

    Get image header

.. c:function:: int spng_get_plte(struct spng_ctx *ctx, struct spng_plte *plte)

    Get image palette

.. c:function:: int spng_get_trns(struct spng_ctx *ctx, struct spng_trns *trns)

    Get image transparency

.. c:function:: int spng_get_chrm(struct spng_ctx *ctx, struct spng_chrm *chrm)

    Get primary chromacities and white point as floating point numbers

.. c:function:: int spng_get_chrm_int(struct spng_ctx *ctx, struct spng_chrm_int *chrm_int)

    Get primary chromacities and white point in PNG's internal representation

.. c:function:: int spng_get_gama(struct spng_ctx *ctx, double *gamma)

    Get image gamma

.. c:function:: int spng_get_iccp(struct spng_ctx *ctx, struct spng_iccp *iccp)

    Get ICC color profile

.. c:function:: int spng_get_sbit(struct spng_ctx *ctx, struct spng_sbit *sbit)

    Get significant bits

.. c:function:: int spng_get_srgb(struct spng_ctx *ctx, uint8_t *rendering_intent)

    Get rendering intent

.. c:function:: int spng_get_text(struct spng_ctx *ctx, struct spng_text *text, uint32_t *n_text)

    Copies text information to ``*text``.

    ``*n_text`` should be greater than or equal to the number of stored text chunks.

    If ``*text`` is NULL and ``*n_text`` is non-NULL then ``*n_text`` is set to the number
    of stored text chunks.

    .. note:: Due to the structure of PNG files it is recommended to call this function
              after :c:func:`spng_decode_image` to retrieve all text chunks.

    .. warning:: Text data is freed when calling :c:func:`spng_ctx_free`.

.. c:function:: int spng_get_bkgd(struct spng_ctx *ctx, struct spng_bkgd *bkgd)

    Get image background color

.. c:function:: int spng_get_hist(struct spng_ctx *ctx, struct spng_hist *hist)

    Get image histogram

.. c:function:: int spng_get_phys(struct spng_ctx *ctx, struct spng_phys *phys)

    Get phyiscal pixel dimensions

.. c:function:: int spng_get_splt(struct spng_ctx *ctx, struct spng_splt *splt, uint32_t *n_splt)

    Copies suggested palettes to ``*splt``.

    ``*n_splt`` should be greater than or equal to the number of stored sPLT chunks.

    If ``*splt`` is NULL and ``*n_splt`` is non-NULL then ``*n_splt`` is set to the number
    of stored sPLT chunks.

    .. warning:: Suggested palettes are freed when calling :c:func:`spng_ctx_free`.

.. c:function:: int spng_get_time(struct spng_ctx *ctx, struct spng_time *time)

    Get modification time

  .. note:: Due to the structure of PNG files it is recommended to call this function
            after :c:func:`spng_decode_image`.


.. c:function:: int spng_get_offs(struct spng_ctx *ctx, struct spng_offs *offs)

    Get image offset

.. c:function:: int spng_get_exif(struct spng_ctx *ctx, struct spng_exif *exif)

    Get EXIF data

  .. note:: Due to the structure of PNG files it is recommended to call this function
            after :c:func:`spng_decode_image`.

  .. warning:: :c:member:`exif.data` is freed when calling :c:func:`spng_ctx_free`.



.. c:function:: int spng_set_ihdr(struct spng_ctx *ctx, struct spng_ihdr *ihdr)

    Set image header

.. c:function:: int spng_set_plte(struct spng_ctx *ctx, struct spng_plte *plte)

    Set image palette

.. c:function:: int spng_set_trns(struct spng_ctx *ctx, struct spng_trns *trns)

    Set transparency

.. c:function:: int spng_set_chrm(struct spng_ctx *ctx, struct spng_chrm *chrm)

    Set primary chromacities and white point as floating point numbers

.. c:function:: int spng_set_chrm_int(struct spng_ctx *ctx, struct spng_chrm_int *chrm_int)

    Set primary chromacities and white point in PNG's internal representation

.. c:function:: int spng_set_gama(struct spng_ctx *ctx, double gamma)

    Set image gamma

.. c:function:: int spng_set_iccp(struct spng_ctx *ctx, struct spng_iccp *iccp)

    Set ICC color profile

    :c:member:`spng_iccp.profile_name` must only contain printable Latin-1 characters and spaces.
    Leading, trailing, and consecutive spaces are not permitted.

.. c:function:: int spng_set_sbit(struct spng_ctx *ctx, struct spng_sbit *sbit)

    Set significant bits

.. c:function:: int spng_set_srgb(struct spng_ctx *ctx, uint8_t rendering_intent)

    Set rendering intent

.. c:function:: int spng_set_text(struct spng_ctx *ctx, struct spng_text *text, uint32_t n_text)

    Set text data

    ``*text`` should point to an :c:type:`spng_text` array of ``n_text`` elements.

    :c:member:`spng_text.text` must only contain Latin-1 characters.
    Newlines must be a single linefeed character (decimal 10).

    :c:member:`spng_text.translated_keyword` must not contain linebreaks.

    :c:member:`spng_text.compression_method` must be zero.

    .. note::

.. c:function:: int spng_set_bkgd(struct spng_ctx *ctx, struct spng_bkgd *bkgd)

    Set image background color

.. c:function:: int spng_set_hist(struct spng_ctx *ctx, struct spng_hist *hist)

    Set image histogram

.. c:function:: int spng_set_phys(struct spng_ctx *ctx, struct spng_phys *phys)

    Set phyiscal pixel dimensions

.. c:function:: int spng_set_splt(struct spng_ctx *ctx, struct spng_splt *splt, uint32_t n_splt)

    Set suggested palette(s).

    ``*splt`` should point to an :c:type:`spng_splt` array of ``n_splt`` elements.

.. c:function:: int spng_set_time(struct spng_ctx *ctx, struct spng_time *time)

    Set modification time

.. c:function:: int spng_set_offs(struct spng_ctx *ctx, struct spng_offs *offs)

    Set image offset

.. c:function:: int spng_set_exif(struct spng_ctx *ctx, struct spng_exif *exif)

    Set EXIF data