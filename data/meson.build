substs = configuration_data()
substs.set('LIBEXECDIR', libexecdir)

service_file = configure_file(
    input: 'low-memory-monitor.service.in',
    output: 'low-memory-monitor.service',
    configuration: substs)
install_data(service_file,
	     install_dir: systemd_system_unit_dir)

install_data('org.freedesktop.LowMemoryMonitor.conf',
	     install_dir: join_paths(get_option('datadir'), 'dbus-1', 'system.d'))
