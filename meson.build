project('libspng', 'c', version : '0.3.1', default_options : ['c_std=c99'])

cc = meson.get_compiler('c')

include = include_directories('.')

zlib_dep = dependency('zlib')
m_dep = cc.find_library('m', required : false)

spng_deps = [ zlib_dep, m_dep ]

lib = library('spng', [ 'decode.c', 'common.c' ],
	          dependencies : spng_deps,
	          install : true)

spng_dep = declare_dependency(link_with : lib,
                              dependencies : spng_deps,
                              include_directories : include,
                              version : meson.project_version())

example = executable('example', 'example.c',
                     link_with : lib,
                     dependencies : zlib_dep)

install_headers('spng.h')

pkg = import('pkgconfig')
pkg.generate(name : 'libspng',
	         libraries : lib,
	         version : meson.project_version(),
	         description : 'PNG decoding and encoding library',
	         libraries_private : [ '-lm', '-lz' ])

dev_build = get_option('dev_build')

if(dev_build)
    subdir('tests')
endif