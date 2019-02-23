add_languages('cpp')
cpp = meson.get_compiler('cpp')

fuzzlib = cpp.find_library('Fuzzer', required : false)

if(fuzzlib.found())
    threads = dependency('threads')

    fuzz_exe = executable('spng_read_fuzzer',
                          'spng_read_fuzzer.cc',
                           link_with : spng_lib,
                           dependencies : [ fuzzlib, threads ])
else
    warning('fuzz target will not be built')
endif

png_dep = dependency('libpng', version : '>=1.6.0')

test_exe = executable('testsuite',
                      'testsuite.c',
                      link_with : spng_lib,
                      include_directories : spng_inc,
                      dependencies : png_dep)

example = executable('example', example_src,
                     link_with : spng_lib,
                     include_directories : spng_inc,
                     dependencies : zlib_dep)

subdir('images')
subdir('crashers')