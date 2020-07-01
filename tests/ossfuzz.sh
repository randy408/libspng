#!/bin/bash -eu

# This script is meant to be run by
# https://github.com/google/oss-fuzz/blob/master/projects/libspng/Dockerfile

mkdir $SRC/libspng/zlib/build $SRC/libspng/build
cd $SRC/libspng/zlib/build
cmake ..
make -j$(nproc) install
ldconfig

cd $SRC/libspng/build
cmake -DSPNG_STATIC=ON -DCMAKE_C_COMPILER_WORKS=1 ..
make -j$(nproc)
cd ..

$CXX $CXXFLAGS -std=c++11 -I. \
    $SRC/libspng/tests/spng_read_fuzzer.cc \
    -o $OUT/spng_read_fuzzer \
    $LIB_FUZZING_ENGINE $SRC/libspng/build/libspng_static.a $SRC/libspng/zlib/build/libz.a

$CXX $CXXFLAGS -std=c++11 -I. \
    $SRC/libspng/tests/spng_read_fuzzer.cc \
    -o $OUT/spng_read_fuzzer_structure_aware \
    -include $SRC/fuzzer-test-suite/libpng-1.2.56/png_mutator.h \
    -D PNG_MUTATOR_DEFINE_LIBFUZZER_CUSTOM_MUTATOR \
    $LIB_FUZZING_ENGINE $SRC/libspng/build/libspng_static.a $SRC/libspng/zlib/build/libz.a

find $SRC/libspng/tests/images -name "*.png" | \
     xargs zip $OUT/spng_read_fuzzer_seed_corpus.zip

cp $SRC/libspng/tests/spng_read_fuzzer.dict $OUT/
