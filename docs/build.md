# Platform requirements

* Requires [zlib](http://zlib.net) or a zlib-compatible library
* Fixed width integer types up to `(u)int32_t`
* Integers must be two's complement.
* `CHAR_BIT` must equal 8.
* `size_t` must be unsigned and at least 32-bit, 16-bit platforms are not
  supported.


# Build with CMake

```bash
mkdir cbuild
cd cbuild
cmake ..
make
make install
```

# Build with Meson

```bash
meson build
cd build
ninja
ninja install
```

# Optimizations

Architecture-specific optimizations are enabled by default,
this can be disabled with the `SPNG_DISABLE_OPT` compiler option.

The Meson project has an `enable_opt` option, it is enabled by default,
the CMake equivalent is `ENABLE_OPT`.

Optimizations on x86 require SSE2 by default, to enable SSSE3
optimizations add `-DSPNG_SSE=3` as a compiler option, this improves
performance by up to 7%.

Compiler-specific macros are used to omit the need for the `-msse2` and
`-mssse3` compiler flags, if the code does not compile without these flags
you should file a bug report.

# Profile-guided optimization

[Profile-guided optimization (PGO)](https://clang.llvm.org/docs/UsersManual.html#profile-guided-optimization)
improves performance by up to 10%.

```bash
# Run in root directory
git clone https://github.com/libspng/benchmark_images.git
cd build
meson configure -Dbuildtype=release --default-library both -Db_pgo=generate
ninja
./example ../benchmark_images/medium_rgb8.png
./example ../benchmark_images/medium_rgba8.png
./example ../benchmark_images/large_palette.png
meson configure -Db_pgo=use
ninja
ninja install
```
