project('libspng', 'c',
    version : '0.6.1',
    license : [ 'BSD-2-Clause', 'libpng-2.0' ],
    default_options : 'c_std=c99'
)

cc = meson.get_compiler('c')
cdata = configuration_data()

cdata.set('SPNG_STATIC', get_option('default_library') == 'static')

cdata.set('SPNG_DISABLE_OPT', not get_option('enable_opt'))

# Check for GNU target_clones attribute
cdata.set('SPNG_ENABLE_TARGET_CLONES',
  cc.compiles(files('tests/target_clones.c'), args : '-Werror', name : 'have target_clones'))

cdata.set('SPNG_USE_MINIZ', get_option('use_miniz'))

static_subproject = cdata.get('SPNG_STATIC', false) and meson.is_subproject()

spng_deps = [ cc.find_library('m', required : false) ]

if cdata.get('SPNG_USE_MINIZ', false)
    spng_deps += dependency('miniz', fallback : [ 'miniz', 'miniz_dep'])
else
    zlib_dep = dependency('zlib',
        required : false,
        fallback : ['zlib', 'zlib_dep'],
        static : get_option('static_zlib'))

    spng_deps += zlib_dep.found() ? zlib_dep : cc.find_library('z')
endif

subdir('include/spng')
spng_inc = include_directories('include')

spng_src = files('src/spng.c')

spng_lib = library('spng',
    spng_src,
    include_directories : spng_inc,
    dependencies : spng_deps,
    install : not static_subproject,
    version : meson.project_version()
)

spng_dep = declare_dependency(
    link_with : spng_lib,
    include_directories : spng_inc,
    version : meson.project_version()
)

if meson.version().version_compare('>= 0.54.0')
    meson.override_dependency('spng', spng_dep)
endif

if meson.is_subproject()
    subdir_done()
endif

install_headers('include/spng.h')

subdir('examples')
subdir('tests')
if get_option('benchmarks')
    subproject('spngt')
endif

pkg = import('pkgconfig')
pkg.generate(spng_lib, description : 'PNG decoding and encoding library')
