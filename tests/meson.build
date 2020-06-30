add_languages('cpp', native : false)
cpp = meson.get_compiler('cpp')

if cc.has_function('fmemopen', prefix : '#include <stdio.h>', args : '-D_GNU_SOURCE')
    repro_exe = executable(
        'fuzz_repro',
        'fuzz_main.c',
        c_args : '-D_GNU_SOURCE',
        link_with : spng_lib)
endif

png_dep = dependency('libpng', version : '>=1.6.0', fallback : ['libpng', 'png_dep'])

test_deps = [ spng_dep, png_dep ]

test_exe = executable('testsuite', 'testsuite.c', dependencies : test_deps)

test('info', test_exe, args : 'info')

cpp_exe = executable('cpp_exe', 'test.cpp', dependencies : spng_dep)
test('cpp_test', cpp_exe)

subdir('images')
subdir('crashers')

if get_option('oss_fuzz') == true
    corpora = subproject('fuzzing_corpora').get_variable('corpora')

    foreach case : corpora
        test('testcase', test_exe, args : case, should_fail : true)
    endforeach
endif