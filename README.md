[![Financial Contributors on Open Collective](https://opencollective.com/libspng/all/badge.svg?label=financial+contributors)](https://opencollective.com/libspng) [![Gitter](https://badges.gitter.im/libspng/community.svg)](https://gitter.im/libspng/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)
[![Windows CI](https://ci.appveyor.com/api/projects/status/raaer7ali0i1v25q?svg=true)](https://ci.appveyor.com/project/randy408/libspng)
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

The goal is to provide a fast PNG library with a simpler API than [libpng](https://github.com/glennrp/libpng/blob/libpng16/png.h).

## Performance

![](https://libspng.org/perfx86.png)

## Features

| Feature                           | libspng            | libpng   | stb_image | lodepng |
|-----------------------------------|--------------------|----------|-----------|---------|
| Decode to RGBA8/16                | ✓                  | ✓        | ✓         | ✓       |
| Decode from stream                | ✓                  | ✓        | ✓         | ❌       |
| Gamma correction                  | ✓                  | ✓        | ❌         | ❌       |
| Fuzzed by [OSS-Fuzz][1]           | ✓                  | ✓        | ❌         | ✓       |
| Progressive read                  | ❌*                 | ✓        | ❌         | ❌       |
| Doesn't require zlib              | ❌                  | ❌        | ✓         | ✓       |
| Encoding                          | ❌*                 | ✓        | ✓         | ✓       |
| Animated PNG                      | ❌*                 | ✓**      | ❌         | ❌      |

[1]: https://github.com/google/oss-fuzz

\* Feature in planning phase

\*\* With a 3rd party patch

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

## Contributors

### Code Contributors

This project exists thanks to all the people who contribute. [[Contribute](CONTRIBUTING.md)].
<a href="https://github.com/randy408/libspng/graphs/contributors"><img src="https://opencollective.com/libspng/contributors.svg?width=890&button=false" /></a>

### Financial Contributors

Become a financial contributor and help us sustain our community. [[Contribute](https://opencollective.com/libspng/contribute)]

#### Individuals

<a href="https://opencollective.com/libspng"><img src="https://opencollective.com/libspng/individuals.svg?width=890"></a>

#### Organizations

Support this project with your organization. Your logo will show up here with a link to your website. [[Contribute](https://opencollective.com/libspng/contribute)]

<a href="https://opencollective.com/libspng/organization/0/website"><img src="https://opencollective.com/libspng/organization/0/avatar.svg"></a>
<a href="https://opencollective.com/libspng/organization/1/website"><img src="https://opencollective.com/libspng/organization/1/avatar.svg"></a>
<a href="https://opencollective.com/libspng/organization/2/website"><img src="https://opencollective.com/libspng/organization/2/avatar.svg"></a>
<a href="https://opencollective.com/libspng/organization/3/website"><img src="https://opencollective.com/libspng/organization/3/avatar.svg"></a>
<a href="https://opencollective.com/libspng/organization/4/website"><img src="https://opencollective.com/libspng/organization/4/avatar.svg"></a>
<a href="https://opencollective.com/libspng/organization/5/website"><img src="https://opencollective.com/libspng/organization/5/avatar.svg"></a>
<a href="https://opencollective.com/libspng/organization/6/website"><img src="https://opencollective.com/libspng/organization/6/avatar.svg"></a>
<a href="https://opencollective.com/libspng/organization/7/website"><img src="https://opencollective.com/libspng/organization/7/avatar.svg"></a>
<a href="https://opencollective.com/libspng/organization/8/website"><img src="https://opencollective.com/libspng/organization/8/avatar.svg"></a>
<a href="https://opencollective.com/libspng/organization/9/website"><img src="https://opencollective.com/libspng/organization/9/avatar.svg"></a>

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

Releases follow the [semantic versioning](https://semver.org/) scheme with a few exceptions:

* Releases from 0.4.0 to 0.8.x are stable
* 0.8.x will be maintained as a separate release branch from 1.0.0

Note that 1.0.0 will introduce little to no breaking changes.

## Documentation

Online documentation is available at [libspng.org/docs](https://libspng.org/docs).

## Known Issues

* Text and iCCP chunks are not read.
* `spng_crc_set_action()` is partially implemented, `SPNG_CRC_DISCARD` has no effect.
