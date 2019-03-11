# Overview

libspng is a C library for reading and writing Portable Network Graphics (PNG) 
format files with a focus on security and ease of use.

libspng is an alternative to libpng, the projects are separate and the APIs are
not compatible.

# Motivation 

The goal is to provide a PNG library with a simpler API than [libpng](https://github.com/glennrp/libpng/blob/libpng16/png.h).

Peformance is also a priority, decoding is 8% faster than libpng for truecolor images with an alpha channel and 3% faster for truecolor images.

The testsuite is designed to test both libraries, it has already uncovered a [bug](https://sourceforge.net/p/libpng/bugs/282/) in libpng.

# Versioning

Releases follow the [semantic versioning](https://semver.org/) scheme with a few exceptions:

* Releases from 0.4.0 to 0.8.x are stable
* 0.8.x will be maintained as a separate release branch from 1.0.0

Note that 1.0.0 will introduce little to no breaking changes.

API/ABI changes can be tracked [here](https://abi-laboratory.pro/index.php?view=timeline&l=libspng).

# Licensing

Code is licensed under the BSD 2-clause "Simplified" License.

The project contains optimizations and test images from libpng, these are licensed under the
[PNG Reference Library License version 2](http://www.libpng.org/pub/png/src/libpng-LICENSE.txt).

# Testing

libspng comes with an extensive test suite. There are over 700 test cases, 
175 [test images](http://www.schaik.com/pngsuite/) are decoded with all possible 
output format and flag combinations and compared against libpng's output.

The testsuite also includes regression tests from libpng and is compiled with 
AddressSanitizer and UndefinedBehaviorSanitizer.

# Security

Code is written according to the rules of the 
[CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard).
All integer arithmetic is checked for overflow and all error conditions are handled gracefully.

The library is continously fuzzed by [OSS-Fuzz](https://github.com/google/oss-fuzz), 
releases are scanned with Clang Static Analyzer, PVS-Studio and Coverity Scan
and have a [Defect Density](https://scan.coverity.com/projects/randy408-libspng) 0.00.

# Documentation

Online documentation is available at [libspng.org/doc](https://libspng.org/doc).

# Known Issues

* Text and iCCP chunks are not read, see issue [#31](https://gitlab.com/randy408/libspng/issues/31).
* `spng_crc_set_action()` is partially implemented, `SPNG_CRC_DISCARD` has no effect.

# [Build from source](https://libspng.org/doc/build)

Download the latest release from [libspng.org/download](https://libspng.org/download).

Meson is the primary build system but CMake is also supported, the library only depends on zlib.

### CMake build

```
mkdir -p cbuild
cd cbuild
cmake ..
make
make install
```

### Meson build

```
meson build
cd build
ninja
ninja install
```

### Amalgamation

The amalgamation is a concatenation of source files, you only have to include `spng.c/spng.h` in your project.
Define `SPNG_OPTIMIZE_DEFILTER` before including `spng.h` to enable optimizations.

## Running the testsuite

Only with Meson builds, the testsuite requires libpng.

```
# Run in build directory
meson configure -Ddev_build=true
ninja test
```