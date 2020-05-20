[![Gitter](https://badges.gitter.im/libspng/community.svg)](https://gitter.im/libspng/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)
[![GitLab CI](https://gitlab.com/randy408/libspng-ci/badges/master/pipeline.svg)](https://gitlab.com/randy408/libspng-ci/pipelines/latest)
[![Fuzzing Status](https://oss-fuzz-build-logs.storage.googleapis.com/badges/libspng.svg)](https://oss-fuzz-build-logs.storage.googleapis.com/index.html#libspng)
[![Coverity](https://scan.coverity.com/projects/15336/badge.svg)](https://scan.coverity.com/projects/randy408-libspng)
[![codecov](https://codecov.io/gl/randy408/libspng/branch/master/graph/badge.svg)](https://codecov.io/gl/randy408/libspng)
[![tag](https://img.shields.io/github/tag-date/randy408/libspng.svg)](https://libspng.org/download.html)

# libspng

libspng is a C library for reading and writing Portable Network Graphics (PNG)
format files with a focus on security and ease of use.

libspng is an alternative to libpng, the projects are separate and the APIs are
not compatible.

## Motivation

The goal is to provide a fast PNG library with a simpler API than [libpng](https://github.com/glennrp/libpng/blob/libpng16/png.h),
it outperforms the reference implementation in common use cases.

## Performance

![](https://libspng.org/perfx86.png)

## Features

| Feature                    | libspng    | libpng   | stb_image | lodepng |
|----------------------------|------------|----------|-----------|---------|
| Decode from stream         | ✅        | ✅       | ✅        | ❌     |
| Gamma correction           | ✅        | ✅       | ❌        | ❌     |
| No known security bugs*    | ✅        | ✅       | ❌        | ✅     |
| Progressive image read     | ✅        | ✅       | ❌        | ❌     |
| Parses all standard chunks | ❌*       | ✅       | ❌        | ❌     |
| Doesn't require zlib       | ❌*       | ❌       | ✅        | ✅     |
| Encoding                   | ❌*       | ✅       | ✅        | ✅     |
| Animated PNG               | ❌*       | ✅***    | ❌        | ❌     |

\* The project is fuzz tested on [OSS-Fuzz](https://github.com/google/oss-fuzz) and vulnerabilities are fixed before they become public.

\* Work in progress

\*\*\* With a 3rd party patch

## Getting spng

Download the [latest release](https://libspng.org/download) and include `spng.c/spng.h` in your project,
you can also build with CMake or Meson. Refer to the [documentation](https://libspng.org/docs) for details.

## Usage

```c
/* Create a context */
spng_ctx *ctx = spng_ctx_new(0);

/* Set an input buffer */
spng_set_png_buffer(ctx, buf, buf_size);

/* Determine output image size */
spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &out_size);

/* Decode to 8-bit RGBA */
spng_decode_image(ctx, out, out_size, SPNG_FMT_RGBA8, 0);

/* Free context memory */
spng_ctx_free(ctx);
```

See [example.c](https://github.com/randy408/libspng/blob/v0.5.0/examples/example.c).

## License

Code is licensed under the BSD 2-clause "Simplified" License.

The project contains optimizations and test images from libpng, these are licensed under the
[PNG Reference Library License version 2](http://www.libpng.org/pub/png/src/libpng-LICENSE.txt).

## Security & Testing

Code is written according to the rules of the
[CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard).
All integer arithmetic is checked for overflow and all error conditions are handled gracefully.

The library is continuously fuzzed by [OSS-Fuzz](https://github.com/google/oss-fuzz),
releases are scanned with Clang Static Analyzer, PVS-Studio, and Coverity Scan.

The test suite consists of over 700 test cases,
175 [test images](http://www.schaik.com/pngsuite/) are decoded with all possible
output format and flag combinations and compared against libpng for correctness.

## Versioning

Releases follow the [semantic versioning](https://semver.org/) scheme with additional guarantees:

* Releases from 0.4.0 to 0.8.x are stable
* If 1.0.0 will introduce breaking changes then 0.8.x will be maintained as a separate stable branch

Currently 1.0.0 is planned to be [compatible](https://github.com/randy408/libspng/issues/3).

## Documentation

Online documentation is available at [libspng.org/docs](https://libspng.org/docs).

## Known Issues

* Text and iCCP chunks are not read.
* Gamma correction is not implemented for `SPNG_FMT_PNG`.
* `spng_crc_set_action()` is partially implemented, `SPNG_CRC_DISCARD` has no effect.
