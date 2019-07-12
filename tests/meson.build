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

test_exe = executable('testsuite', 'testsuite.c', dependencies : [ spng_dep, png_dep ])

test('info', test_exe, args : 'info')

subdir('images')
subdir('crashers')