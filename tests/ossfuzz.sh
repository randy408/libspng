#!/bin/bash -eu

# This script is meant to be run by
# https://github.com/google/oss-fuzz/blob/master/projects/libspng/Dockerfile

mkdir $SRC/libspng/zlib/build $SRC/libspng/build
cd $SRC/libspng/zlib/build
cmake ..
make -j$(nproc)

$CC $CFLAGS -c -I$SRC/libspng/zlib/build -I$SRC/libspng/zlib \
    $SRC/libspng/spng/spng.c \
    -o $SRC/libspng/build/libspng.o \

cp $SRC/libspng/zlib/build/libz.a $SRC/libspng/build/libz.a
cd $SRC/libspng/build
ar x libz.a
ar rcs libspng_static.a *.o

$CXX $CXXFLAGS -std=c++11 \
    $SRC/libspng/tests/spng_read_fuzzer.cc \
    -o $OUT/spng_read_fuzzer \
    $LIB_FUZZING_ENGINE $SRC/libspng/build/libspng_static.a $SRC/libspng/zlib/build/libz.a

$CXX $CXXFLAGS -std=c++11 -I$SRC/libspng/zlib/build -I$SRC/libspng/zlib \
    $SRC/libspng/tests/spng_read_fuzzer.cc \
    -o $OUT/spng_read_fuzzer_structure_aware \
    -include $SRC/fuzzer-test-suite/libpng-1.2.56/png_mutator.h \
    -D PNG_MUTATOR_DEFINE_LIBFUZZER_CUSTOM_MUTATOR \
    $LIB_FUZZING_ENGINE $SRC/libspng/build/libspng_static.a $SRC/libspng/zlib/build/libz.a

find $SRC/libspng/tests/images -name "*.png" | \
     xargs zip $OUT/spng_read_fuzzer_seed_corpus.zip

cp $SRC/libspng/tests/spng_read_fuzzer.dict $OUT/
