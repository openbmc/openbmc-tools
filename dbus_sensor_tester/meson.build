project('dbus-sensor-tester', 'cpp',
        version : '1.0',
        meson_version: '>=1.1.1',
        default_options: [
            'b_lto_mode=default',
            'b_lto_threads=0',
            'b_lto=true',
            'b_ndebug=if-release',
            'buildtype=debugoptimized',
            'cpp_rtti=false',
            'cpp_std=c++23',
            'warning_level=3',
            'werror=true',
           ])

# Project related links

project_pretty_name = 'dbus-sensor-tester'

# Validate the c++ Standard

if get_option('cpp_std') != 'c++23'
    error('This project requires c++23 support')
endif

# Get compiler and default build type

cxx = meson.get_compiler('cpp')
build = get_option('buildtype')
optimization = get_option('optimization')
summary('Build Type',build, section : 'Build Info')
summary('Optimization',optimization, section : 'Build Info')

# Disable lto when compiling with no optimization
if(get_option('optimization') == '0')
  add_project_arguments('-fno-lto', language: 'cpp')
  message('Disabling lto & its supported features as optimization is disabled')
endif

# Add compiler arguments

# -Wpedantic, -Wextra comes by default with warning level
add_project_arguments(
  cxx.get_supported_arguments([
  '-Wcast-align',
  '-Wconversion',
  '-Wformat=2',
  '-Wold-style-cast',
  '-Woverloaded-virtual',
  '-Wsign-conversion',
  '-Wunused',
  '-Wno-attributes',
  ]),
  language: 'cpp'
)

if (cxx.get_id() == 'clang' and cxx.version().version_compare('>9.0'))
add_project_arguments(
  cxx.get_supported_arguments([
    '-Weverything',
    '-Wno-c++98-compat-pedantic',
    '-Wno-c++98-compat',
    '-Wno-documentation-unknown-command',
    '-Wno-documentation',
    '-Wno-exit-time-destructors',
    '-Wno-global-constructors',
    '-Wno-newline-eof',
    '-Wno-padded',
    '-Wno-shadow',
    '-Wno-used-but-marked-unused',
    '-Wno-weak-vtables',
  ]),
  language:'cpp')
endif

# if compiler is gnu-gcc , and version is > 8.0 then we add few more
# compiler arguments , we know that will pass

if (cxx.get_id() == 'gcc' and cxx.version().version_compare('>8.0'))
  add_project_arguments(
    cxx.get_supported_arguments([
     '-Wduplicated-cond',
     '-Wduplicated-branches',
     '-Wlogical-op',
     '-Wunused-parameter',
     '-Wnull-dereference',
     '-Wdouble-promotion',
     ]),
    language:'cpp')
endif

# Find the dependency modules, if not found use meson wrap to get them
# automatically during the configure step
dependencies = []

sdbusplus = dependency('sdbusplus', required : false, include_type: 'system')
if not sdbusplus.found()
  sdbusplus_proj = subproject('sdbusplus', required: true)
  sdbusplus = sdbusplus_proj.get_variable('sdbusplus_dep')
  sdbusplus = sdbusplus.as_system('system')
endif
dependencies += sdbusplus

cli11 = dependency('cli11', required : false, include_type: 'system')
if not cli11.found()
  cli11_proj = subproject('cli11', required: true)
  cli11 = cli11_proj.get_variable('CLI11_dep')
  cli11 = cli11.as_system('system')
endif
dependencies += cli11

systemd = dependency('systemd')
dependencies += [systemd]

boost = dependency('boost', version : '>=1.78.0', required : false, include_type: 'system')
if not boost.found()
  subproject('boost', required: false)
  boost_inc = include_directories('subprojects/boost_1_78_0/', is_system:true)
  boost  = declare_dependency(include_directories : boost_inc)
  boost = boost.as_system('system')
endif
dependencies += boost

srcfiles_sensortest = [
]

systemd_system_unit_dir = systemd.get_variable('systemdsystemunitdir')
bindir = get_option('prefix') + '/' +get_option('bindir')

# Generate the executable
executable(
  'sensortest',
  srcfiles_sensortest + ['main.cpp'],
  dependencies: dependencies,
  link_args: '-Wl,--gc-sections',
  install: true,
  install_dir: bindir
)
