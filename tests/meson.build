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

png_dep = dependency('libpng', version : '>=1.6.0', fallback : ['libpng', 'png_dep'])

test_deps = [ spng_dep, png_dep ]

test_exe = executable('testsuite', 'testsuite.c', dependencies : test_deps)

test('info', test_exe, args : 'info')

cpp_exe = executable('cpp_exe', 'test.cpp', dependencies : spng_dep)
test('cpp_test', cpp_exe)

subdir('images')
subdir('crashers')