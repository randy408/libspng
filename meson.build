project('libspng', 'c',
    version : '0.6.2',
    license : [ 'BSD-2-Clause', 'libpng-2.0' ],
    default_options : [
        'b_lto=true',
        'b_ndebug=if-release',
        'buildtype=debugoptimized',
        'c_std=c99',
        'warning_level=3',
    #   'werror=true',
    ],
)

spng_args = []
static_subproject = false

if get_option('default_library') == 'static'
    spng_args = '-DSPNG_STATIC'
    static_subproject = meson.is_subproject()
endif

if not get_option('enable_opt')
    add_project_arguments('-DSPNG_DISABLE_OPT', language : 'c')
endif

cc = meson.get_compiler('c')

# Check for GNU target_clones attribute
if cc.compiles(files('tests/target_clones.c'), args : '-Werror', name : 'have target_clones')
    add_project_arguments('-DSPNG_ENABLE_TARGET_CLONES', language : 'c')
endif

spng_deps = [ cc.find_library('m', required : false) ]

if get_option('use_miniz')
    add_project_arguments('-DSPNG_USE_MINIZ', language : 'c')
    spng_deps += dependency('miniz',
        default_options : [ 'default_library=static' ],
        fallback : [ 'miniz', 'miniz_dep' ],
    )
else
    zlib_dep = dependency('zlib',
        default_options : [ 'default_library=static' ],
        fallback : [ 'zlib', 'zlib_dep' ],
        required : false,
        static : get_option('static_zlib'),
    )
    spng_deps += zlib_dep.found() ? zlib_dep : cc.find_library('z')
endif

spng_inc = include_directories('spng')

spng_src = files('spng/spng.c')

spng_lib = library('spng', spng_src,
    c_args : spng_args,
    dependencies : spng_deps,
    install : not static_subproject,
    version : meson.project_version()
)

spng_dep = declare_dependency(
    compile_args : spng_args,
    include_directories : spng_inc,
    link_with : spng_lib,
)

if meson.version().version_compare('>= 0.54.0')
    meson.override_dependency('spng', spng_dep)
endif

if meson.is_subproject()
    subdir_done()
endif

install_headers('spng/spng.h')

subdir('examples')

if get_option('dev_build')
    subdir('tests')
endif

if get_option('benchmarks')
    subproject('spngt')
endif

if static_subproject
    subdir_done()
endif

pc= import('pkgconfig')
pc.generate(spng_lib,
    description : 'PNG decoding and encoding library',
    extra_cflags : spng_args,
)
