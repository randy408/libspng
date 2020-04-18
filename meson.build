project('libspng', 'c', version : '0.6.0', license : 'bsd', default_options : ['c_std=c99'])

cc = meson.get_compiler('c')

spng_inc = include_directories('.')

if cc.get_define('__ANDROID__') != ''
    zlib_dep = cc.find_library('z')
else
    zlib_dep = dependency('zlib', fallback : ['zlib', 'zlib_dep'], static : get_option('static_zlib'))
endif

m_dep = cc.find_library('m', required : false)

spng_deps = [ zlib_dep, m_dep ]

if get_option('multithreading') == true
    spng_deps += dependency('threads')
    add_project_arguments('-DSPNG_MULTITHREADING', language : 'c')
endif

if get_option('enable_opt') == false
    add_project_arguments('-DSPNG_DISABLE_OPT', language : 'c')
endif

static_subproject = false
if get_option('default_library') == 'static'
    static_subproject = meson.is_subproject()
    add_project_arguments('-DSPNG_STATIC', language : 'c')
endif

if host_machine.cpu_family() == 'x86_64'
# clang evaluates __has_attribute(target_clones) to true but issues a warning
# about not supporting it when it's used.
    if cc.get_id() != 'clang' and cc.compiles('__attribute__((target_clones("default,avx2"))) int f(int x){return x + 3;}int main(){}', name : 'have target_clones')
        add_project_arguments('-DSPNG_ENABLE_TARGET_CLONES', language : 'c')
    endif
endif

spng_src = files('spng.c')

spng_lib = library(
    'spng', spng_src,
    dependencies : spng_deps,
    version : meson.project_version(),
    soversion : '0.6.0',
    install : not static_subproject)

spng_dep = declare_dependency(
    link_with : spng_lib,
    dependencies : spng_deps,
    include_directories : spng_inc,
    version : meson.project_version())

example_src = files('examples/example.c')

example_exe = executable('example', example_src, dependencies : spng_dep)

if not static_subproject
    install_headers('spng.h')

    pkg = import('pkgconfig')
    pkg.generate(
        name : 'spng',
        libraries : spng_lib,
        version : meson.project_version(),
        description : 'PNG decoding and encoding library',
        libraries_private : [ '-lm', '-lz' ])
endif

if get_option('dev_build') == true
    subdir('tests')
endif

if get_option('benchmarks') == true
    if meson.version().version_compare('>= 0.54.0')
        meson.override_dependency('spng', spng_dep)
        spngt = subproject('spngt')
    else
        warning('benchmarks require Meson >= 0.54.0')
    endif
endif