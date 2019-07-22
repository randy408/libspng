.. _build:

Build
=====

Platform requirements
---------------------

* Requires `zlib <http://zlib.net>`_
* Fixed width integer types up to ``(u)int32_t``
* Integers must be two's complement.
* ``CHAR_BIT`` must equal 8.
* ``size_t`` must be unsigned and at least 32-bit, 16-bit platforms are not
  supported.


Build with Meson
----------------

.. code-block:: bash

   meson build
   cd build
   ninja
   ninja install


Optimizations
--------------------

Architecture-specific optimizations are enabled by default,
these can be disabled with the ``SPNG_DISABLE_OPT`` compiler option.

The Meson project has an ``enable_opt`` option, it is enabled by default,
the CMake equivalent is ``ENABLE_OPT``.

These optimizations require SSE2/SSSE3 on x86 and NEON on ARM.
Compiler-specific macros are used to omit the need for the `-msse2` and
`-mssse3` compiler flags, if the code does not compile without these flags
you should file a bug report.

Profile-guided optimization (PGO)
---------------------------------

`Profile-guided optimization (PGO)
<https://clang.llvm.org/docs/UsersManual.html#profile-guided-optimization>`_
improves performance by up to 10%.

.. note:: Run in root directory

.. code-block:: bash

    git clone https://gitlab.com/randy408/benchmark_images.git
    cd build
    meson configure -Dbuildtype=release --default-library both -Db_pgo=generate
    ninja
    ./example ../benchmark_images/medium_rgb8.png
    ./example ../benchmark_images/medium_rgba8.png
    ./example ../benchmark_images/large_palette.png
    meson configure -Db_pgo=use
    ninja
    ninja install
