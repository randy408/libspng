#!/bin/bash -eu
# Copyright 2019 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################

# This script is meant to be run by
# https://github.com/google/oss-fuzz/blob/master/projects/libspng/Dockerfile

if [ "$ARCHITECTURE" = 'i386' ]; then
    wget http://ftp.us.debian.org/debian/pool/main/z/zlib/zlib1g-dev_1.2.11.dfsg-1_i386.deb http://ftp.us.debian.org/debian/pool/main/z/zlib/zlib1g_1.2.11.dfsg-1_i386.deb
    ln -s /usr/lib/i386-linux-gnu/libz.a $SRC/libspng/libz.a
else
    wget http://ftp.us.debian.org/debian/pool/main/z/zlib/zlib1g-dev_1.2.11.dfsg-1_amd64.deb http://ftp.us.debian.org/debian/pool/main/z/zlib/zlib1g_1.2.11.dfsg-1_amd64.deb
    ln -s /usr/lib/x86_64-linux-gnu/libz.a $SRC/libspng/libz.a
fi

dpkg -i zlib*.deb

meson --default-library=static --buildtype=plain -Dstatic_zlib=true build

ninja -C build

$CXX $CXXFLAGS -std=c++11 -I. \
    $SRC/libspng/tests/spng_read_fuzzer.cc \
    -o $OUT/spng_read_fuzzer \
    $LIB_FUZZING_ENGINE $SRC/libspng/build/libspng.a $SRC/libspng/libz.a

$CXX $CXXFLAGS -std=c++11 -I. \
    $SRC/libspng/tests/spng_read_fuzzer.cc \
    -o $OUT/spng_read_fuzzer_structure_aware \
    -include ../fuzzer-test-suite/libpng-1.2.56/png_mutator.h \
    -D PNG_MUTATOR_DEFINE_LIBFUZZER_CUSTOM_MUTATOR \
    $LIB_FUZZING_ENGINE $SRC/libspng/build/libspng.a $SRC/libspng/libz.a

find $SRC/libspng/tests/images -name "*.png" | \
     xargs zip $OUT/spng_read_fuzzer_seed_corpus.zip

cp $SRC/libspng/tests/spng.dict \
   $SRC/libspng/tests/spng_read_fuzzer.options $OUT/
