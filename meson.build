project('libspng', 'c', version : '0.5.1', license : 'bsd', default_options : ['c_std=c99'])

cc = meson.get_compiler('c')

spng_inc = include_directories('.')

if cc.get_define('__ANDROID__') != ''
    zlib_dep = cc.find_library('z')
else
    zlib_dep = dependency('zlib', fallback : ['zlib', 'zlib_dep'])
endif

m_dep = cc.find_library('m', required : false)

spng_deps = [ zlib_dep, m_dep ]

if get_option('enable_mt') == true
    spng_deps += dependency('threads')
    add_project_arguments('-DSPNG_ENABLE_MT', language : 'c')
endif

if get_option('enable_opt') == false
    add_project_arguments('-DSPNG_DISABLE_OPT', language : 'c')
endif

if get_option('default_library') == 'static'
    add_project_arguments('-DSPNG_STATIC', language : 'c')
endif

install_headers('spng.h')

spng_src = files('spng.c')

spng_lib = library('spng', spng_src,
                   dependencies : spng_deps,
                   version : meson.project_version(),
                   soversion : '0.5.1',
                   install : true)

spng_dep = declare_dependency(link_with : spng_lib,
                              dependencies : spng_deps,
                              include_directories : spng_inc,
                              version : meson.project_version())

example_src = files('examples/example.c')

example_exe = executable('example', example_src, dependencies : spng_dep)

pkg = import('pkgconfig')
pkg.generate(name : 'spng',
             libraries : spng_lib,
             version : meson.project_version(),
             description : 'PNG decoding and encoding library',
             libraries_private : [ '-lm', '-lz' ])

if get_option('dev_build') == true
    subdir('tests')
endif
