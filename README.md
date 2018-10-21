# Overview

libspng is a C library for reading and writing Portable Network Graphics (PNG) 
format files with a focus on security and ease of use.

libspng is an alternative to libpng, the projects are separate and the APIs are
not compatible.

# Versioning

Releases follow the [semantic versioning](https://semver.org/) scheme, new releases until 1.0.0 may introduce breaking changes.

# Licensing

Code is licensed under the BSD 2-clause "Simplified" License.

# Testing

Over 150 test images of different bit depth, color type, and size combinations 
are verified to decode to the same output as libpng.

# Safety

Code is written according to the rules of the 
[CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard),
all releases are scanned with [Coverity Scan](https://scan.coverity.com/projects/randy408-libspng)
and Clang Static Analyzer.

# Documentation

Online documentation is available at https://libspng.org/doc.

## Building from source

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

## Running the testsuite

Only with Meson builds, the testsuite requires libpng.

```
# Run in build directory
meson configure -Ddev_build=true
ninja test
```

## Benchmarking

```
# Run in source directory
git checkout tags/v0.3.1
wget https://gitlab.com/randy408/libspng/snippets/1739147/raw?inline=false -O v030_opt.patch
git apply v030_opt.patch
git clone https://gitlab.com/randy408/benchmark_images.git
cd benchmark_images;
git checkout tags/v0.3.1
cd ../build
meson configure -Dbuildtype=release -Db_pgo=generate
ninja benchmark
meson configure -Db_pgo=use
ninja benchmark
cat meson-logs/benchmarklog.txt
```
