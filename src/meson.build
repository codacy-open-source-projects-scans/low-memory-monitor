resources_sources = gnome.compile_resources(
  'low-memory-monitor-resources',
  'low-memory-monitor.gresource.xml',
  export: true,
  source_dir: '.',
  c_name: 'low_memory_monitor'
)

cflags = [
  common_cflags,
  '-DSYSCONFDIR="@0@"'.format(get_option('sysconfdir')),
  '-DTRIGGER_KERNEL_OOM=@0@'.format(get_option('trigger_kernel_oom').to_int()),
]

executable('low-memory-monitor',
	   [ 'low-memory-monitor.c',
	     'memory-pressure-monitor.c',
	     'lock-memory.c',
	     'sysrq-oom.c',
	     resources_sources ],
	   dependencies: [ glib_dep ],
	   c_args: cflags,
	   install: true,
	   install_dir: libexecdir)

executable('fill-memory',
	   [ 'fill-memory.c' ],
	   dependencies: [ glib_dep ],
	   install: false)
