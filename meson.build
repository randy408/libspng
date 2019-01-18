project('libspng', 'c', version : '0.4.1', license : 'bsd', default_options : ['c_std=c99'])

cc = meson.get_compiler('c')

include = include_directories('src')

zlib_dep = dependency('zlib')
m_dep = cc.find_library('m', required : false)

spng_deps = [ zlib_dep, m_dep ]

lib_src = files(['src/decode.c',
                 'src/common.c'])

optimize_defilter = get_option('optimize_defilter')

if optimize_defilter == true
    if host_machine.cpu_family() == 'x86' or host_machine.cpu_family() == 'x86_64'
        lib_src += files('src/filter_sse2_intrinsics.c')
        add_project_arguments('-DSPNG_OPTIMIZE_DEFILTER', '-msse2', '-mssse3',  language : 'c')
    endif
endif

lib = library('spng', lib_src,
	          dependencies : spng_deps,
	          version : meson.project_version(),
	          soversion : '0.4.1',
	          install : true)

spng_dep = declare_dependency(link_with : lib,
                              dependencies : spng_deps,
                              include_directories : include,
                              version : meson.project_version())

example = executable('example', 'examples/example.c',
                     link_with : lib,
                     include_directories : include,
                     dependencies : zlib_dep)

install_headers('src/spng.h')

pkg = import('pkgconfig')
pkg.generate(name : 'spng',
	         libraries : lib,
	         version : meson.project_version(),
	         description : 'PNG decoding and encoding library',
	         libraries_private : [ '-lm', '-lz' ])

dev_build = get_option('dev_build')

if(dev_build)
    subdir('tests')
endif

