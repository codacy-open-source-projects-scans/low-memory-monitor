version_conf = configuration_data()
version_conf.set('VERSION', meson.project_version())

ifaces = [
  ['org.freedesktop.LowMemoryMonitor', 'org.freedesktop.LowMemoryMonitor.xml']
]
foreach iface : ifaces
  custom_target(iface[0] + ' docs',
    input: '../src/@0@.xml'.format(iface[0]),
    output: 'docs-' + iface[1],
    command: [
      'gdbus-codegen',
      '--interface-prefix=@0@.'.format(iface[0]),
      '--generate-docbook', 'docs',
      '--output-directory', '@OUTDIR@',
      '@INPUT@'
    ],
    build_by_default: true
  )
endforeach

configure_file(
  input: 'version.xml.in',
  output: 'version.xml',
  configuration: version_conf
)

private_headers = [
  'memory-pressure-monitor.h',
  'low-memory-monitor-resources.h',
  'lock-memory.h',
  'sysrq-oom.h'
]

gnome.gtkdoc('low-memory-monitor',
  main_xml: 'low-memory-monitor-docs.xml',
  src_dir: [
    join_paths(meson.source_root(), 'src'),
    join_paths(meson.build_root(), 'src')
  ],
  mkdb_args: [
               '--xml-mode',
               '--output-format=xml'
             ],
  scan_args: [
               '--ignore-headers=' + ' '.join(private_headers),
             ],
  content_files: '../COPYING',
  install: true
)
