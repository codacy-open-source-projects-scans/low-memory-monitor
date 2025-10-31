project('low-memory-monitor', [ 'c' ],
        version: '2.1',
        license: 'GPL-3.0-or-later',
        default_options: [
          'buildtype=debugoptimized',
          'warning_level=1',
          'c_std=c99',
        ],
        meson_version: '>= 0.45.0')

cc = meson.get_compiler('c')

common_cflags = cc.get_supported_arguments([
    '-fgnu89-inline',
    '-fvisibility=hidden',
    '-std=gnu99',
    '-Wall',
    '-Wundef',
    '-Wunused',
    '-Wstrict-prototypes',
    '-Werror-implicit-function-declaration',
    '-Wno-pointer-sign',
    '-Wshadow',
    '-Wtautological-type-limit-compare',
])

gnome = import('gnome')

systemd_system_unit_dir = get_option('systemdsystemunitdir')
if systemd_system_unit_dir == 'auto'
    systemd_dep = dependency('systemd')
    systemd_system_unit_dir = systemd_dep.get_pkgconfig_variable('systemdsystemunitdir')
endif

prefixdir = get_option('prefix')
libexecdir = join_paths(prefixdir, get_option('libexecdir'))

glib_dep = dependency('gio-2.0')

subdir('data')
subdir('src')
if get_option('gtk_doc')
  subdir('docs')
endif
