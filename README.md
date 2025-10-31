# Low Memory Monitor

The Low Memory Monitor is an early boot daemon that will monitor memory
pressure information coming from the kernel, and, when memory pressure means
that memory isn't as readily available and would cause interactivity problems, would:

- send D-Bus signals to user-space applications when memory is running low,
- if configured to do so and memory availability worsens, activate the kernel's
   [OOM](https://en.wikipedia.org/wiki/Out_of_memory) killer.

It is designed for use on traditional Linux systems, with interactive user interfaces
and D-Bus communication.

## Application/Service Developers

Developers of applications and services are encouraged to listen to the signals
sent by the `low-memory-monitor`, either [directly](https://hadess.pages.freedesktop.org/low-memory-monitor/),
or using the [GMemoryMonitor API](https://developer.gnome.org/gio/2.64/GMemoryMonitor.html)
in glib.

If your system or user services/applications are handled by systemd, you'll want
to make sure that the [OOMPolicy](https://www.freedesktop.org/software/systemd/man/systemd.service.html#OOMPolicy=)
and the [OOMScoreAdjust](https://www.freedesktop.org/software/systemd/man/systemd.exec.html#OOMScoreAdjust=)
options are correctly set for your service.

## Signal thresholds

The memory pressure thresholds in low-memory-monitor are not configurable, whether
by users or administrators. The values are based on how long it takes for the
kernel to allocate and aggregate memory at any given point, such that reaching
those values would cause stutters, at the beginning of a memory shortage, or
longer freezes not very long after.

In this context, the threshold we set as the trigger level is much more of a
human decision than anything else - you just need to avoid letting the user
at their keyboard perceive that their system has got into an unrecoverable state
such that they need to pull the plug, and that perception should be independent
of whether they have a fast or slow machine.

If interactivity is not the goal, there are better solutions available for memory
management of batch jobs. See below for references.

## Kernel Out-Of-Memory Killer

The kernel out-of-memory killer is usually triggered by the low-memory-monitor
when physical memory is at its scarcest.

This functionality is disabled by default, unless the distributor changed that
default at compile-time by passing `-Dtrigger_kernel_oom=true` to meson.

The user/admin can also modify `/etc/low-memory-monitor.conf`'s contents to
enable or disable it. For example:

```ini
[Configuration]
TriggerKernelOom=true
```

Or:

```ini
[Configuration]
TriggerKernelOom=false
```

## Background

### Memory Pressure Kernel API

low-memory-monitor uses the kernel's [pressure-stall information](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/accounting/psi.rst).
More information is available in this [LWN article](https://lwn.net/Articles/759781/).

### Endless OS' psi-monitor

The inspiration for low-memory-monitor. The memory monitoring is used to activate
the kernel OOM killer when required. It doesn't help applications use less
memory.

[source](https://github.com/endlessm/eos-boot-helper/tree/master/psi-monitor)

### Android's lowmemorykiller daemon

It incorporates a memory pressure monitor (using the new psi-pressure API, and
“by hand”), and a user-space memory killer built for Android's process model.
Generating the [onTrimMemory](https://developer.android.com/reference/android/content/ComponentCallbacks2.html#onTrimMemory(int))
signal is done somewhere else.

[Documentation](https://source.android.com/devices/tech/perf/lmkd) ([source](https://android.googlesource.com/platform/system/memory/lmkd/))

### Facebook's oomd

A highly customisable user-space out-of-memory killer, primarily built for server
workloads.

[source](https://github.com/facebookincubator/oomd) ([PDF for oomd2](https://linuxplumbersconf.org/event/4/contributions/292/attachments/285/477/LPC_oomd2_and_beyond__a_year_of_improvements.pdf))

### earlyoom

Another user-space out-of-memory killer, built for desktop Linux.

[source](https://github.com/rfjakob/earlyoom)

### WebKit's memory pressure monitor

An example of monitoring memory usage in a Linux desktop application.

[UI process](https://github.com/WebKit/webkit/blob/master/Source/WebKit/UIProcess/linux/MemoryPressureMonitor.cpp)

[Web process](https://github.com/WebKit/webkit/blob/master/Source/WTF/wtf/linux/MemoryPressureHandlerLinux.cpp)

### Why low-memory-monitor

After reading the above description, you should be able to see that none of the
solutions available to us would have brought the features we wanted to have in
a memory monitor, whether it was:

- the lack of communication with user-space processes before activating an
  out-of-memory killer,
- the fact that they were built for different process models or workloads,
- or even that they implemented a user-space out-of-memory killer themselves
  rather than relying on the kernel's.

## Dependencies

Requires Linux 5.2 or newer, GLib and systemd.
