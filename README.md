# Overview

libspng is a C library for reading and writing Portable Network Graphics (PNG) 
format files with a focus on security and ease of use.

# Compatibility

Over 150 test images of different bit depth, color type, and size combinations 
are verified to decode to the same output as libpng.

# Safety

spng does verify input data, it should be free of arithmetic overflow and buffer 
overflow errors.

# Usage

See [example.c](https://gitlab.com/randy408/libspng/blob/master/example.c)


# Build instructions

Building requires meson and ninja, zlib is a dependency and the testsuite also 
requires libpng.

## Building
```
meson build
cd build
ninja
```

## Running the testsuite
```
ninja test
```

## Benchmark

```
git clone --depth=1 https://gitlab.com/randy408/spng_images.git benchmark_images
cd build
meson configure -Dbuildtype=release
ninja benchmark
```
