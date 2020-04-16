add_languages('cpp')
cpp = meson.get_compiler('cpp')

repro_exe = executable('fuzz_repro', 'fuzz_main.c', link_with : spng_lib)

png_dep = dependency('libpng', version : '>=1.6.0', fallback : ['libpng', 'png_dep'])

test_deps = [ spng_dep, png_dep ]

test_exe = executable('testsuite', 'testsuite.c', dependencies : test_deps)

test('info', test_exe, args : 'info')

cpp_exe = executable('cpp_exe', 'test.cpp', dependencies : spng_dep)
test('cpp_test', cpp_exe)

subdir('images')
subdir('crashers')

if get_option('benchmarks') == true
    if meson.version().version_compare('>= 0.54.0')
        meson.override_dependency('spng', spng_dep)
        spngt = subproject('spngt')
    else
        warning('benchmarks require Meson >= 0.54.0')
    endif
endif