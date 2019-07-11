project('libspng', 'c', version : '0.4.6', license : 'bsd', default_options : ['c_std=c99'])

cc = meson.get_compiler('c')

spng_inc = include_directories('.')

zlib_dep = dependency('zlib', fallback : ['zlib', 'zlib_dep'])
m_dep = cc.find_library('m', required : false)

spng_deps = [ zlib_dep, m_dep ]

if get_option('optimize_filter') == true
    add_project_arguments('-DSPNG_OPTIMIZE_FILTER', language : 'c')
endif

install_headers('spng.h')

spng_src = files('spng.c')

spng_lib = library('spng', spng_src,
                   dependencies : spng_deps,
                   version : meson.project_version(),
                   soversion : '0.4.6',
                   install : true)

spng_dep = declare_dependency(link_with : spng_lib,
                              dependencies : spng_deps,
                              include_directories : spng_inc,
                              version : meson.project_version())

example_src = files('examples/example.c')

example_exe = executable('example', example_src,
                         link_with : spng_lib,
                         include_directories : spng_inc,
                         dependencies : zlib_dep)

pkg = import('pkgconfig')
pkg.generate(name : 'spng',
             libraries : spng_lib,
             version : meson.project_version(),
             description : 'PNG decoding and encoding library',
             libraries_private : [ '-lm', '-lz' ])

if get_option('dev_build') == true
    subdir('tests')
endif