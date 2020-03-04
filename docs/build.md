# Platform requirements

* Requires [zlib](http://zlib.net) or a zlib-compatible library
* Integers must be two's complement.
* Fixed width integer types up to `(u)int32_t`
* `CHAR_BIT` must equal 8.
* `size_t` must be unsigned.
* `size_t` and `int` must be at least 32-bit, 16-bit platforms are not
supported.
* Floating point support and math functions

# Build

## CMake

```bash
mkdir cbuild
cd cbuild
cmake .. # Don't forget to set optimization level!
make
make install
```

## Meson

```bash
meson build --buildtype=release # Default is debug
cd build
ninja
ninja install
```

## Embedding the source code

The sources `spng.c`/`spng.h` can be dropped in a project without
any configuration, intrinsics are enabled by default.

# Build options

Architecture-specific intrinsics are enabled by default,
this can be disabled with the `SPNG_DISABLE_OPT` compiler option.

For the Meson project it is controlled with the `enable_opt` option,
the CMake equivalent is `ENABLE_OPT`, they are enabled by default.

Intrinsics for x86 require SSE2, to enable SSSE3 optimizations
add `-DSPNG_SSE=3` as a compiler option, this improves performance by ~7%.

Compiler-specific macros are used to omit the need for the `-msse2` and
`-mssse3` compiler flags, if the code does not compile without these flags
you should file a bug report.

The `target_clones()` function attribute is used to optimize code
for multiple instruction sets, this is enabled by the
`SPNG_ENABLE_TARGET_CLONES` compiler option, it requires a recent version
of GCC and glibc.
For the Meson project this is always enabled if the target supports it.

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
