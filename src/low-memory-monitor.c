/*
 * Copyright (C) 2018 Endless Mobile, Inc.
 * Copyright (c) 2019 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <err.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "memory-pressure-monitor.h"
#include "lock-memory.h"
#include "sysrq-oom.h"
#include "low-memory-monitor-resources.h"

typedef enum {
    LOW_MEMORY_LEVEL_INVALID = -1,
    LOW_MEMORY_LEVEL_LOW = 0,
    LOW_MEMORY_LEVEL_MEDIUM,
    LOW_MEMORY_LEVEL_CRITICAL,
    LOW_MEMORY_LEVEL_COUNT
} LowMemoryLevelEnum;

/* Above this percentage, low memory signals are not sent */
#define AVAILABLE_MEM_RATIO 0.5

/* In seconds */
#define POLL_INTERVAL       1
#define RECOVERY_INTERVAL  15

static struct {
    MemoryPressureMonitorTriggerType trigger_type;
    int threshold_ms;
} triggers[LOW_MEMORY_LEVEL_COUNT] = {
    { MEMORY_PRESSURE_MONITOR_TRIGGER_SOME, 70 },   /* 70ms out of 1sec for partial stall */
    { MEMORY_PRESSURE_MONITOR_TRIGGER_SOME, 100  }, /* 100ms out of 1sec for partial stall */
    { MEMORY_PRESSURE_MONITOR_TRIGGER_FULL, 100 },  /* 100ms out of 1sec for complete stall */
};

#define LOW_MEMORY_MONITOR_DBUS_NAME          "org.freedesktop.LowMemoryMonitor"
#define LOW_MEMORY_MONITOR_DBUS_PATH          "/org/freedesktop/LowMemoryMonitor"
#define LOW_MEMORY_MONITOR_IFACE_NAME         LOW_MEMORY_MONITOR_DBUS_NAME

typedef struct {
    GMainLoop *loop;
    GDBusNodeInfo *introspection_data;
    GDBusConnection *connection;
    guint name_id;
    int ret;

    /* Configuration */
    gboolean trigger_kernel_oom;

    /* For each of the warning levels */
    guint source_id[LOW_MEMORY_LEVEL_COUNT];
    gint64 last_trigger[LOW_MEMORY_LEVEL_COUNT];
    guint signal_timeout;
    LowMemoryLevelEnum current_state;
    LowMemoryLevelEnum next_state;
} LowMemoryMonitorData;

static const char *
levels_str (LowMemoryLevelEnum level)
{
    const char *str[LOW_MEMORY_LEVEL_COUNT] = {
	[LOW_MEMORY_LEVEL_LOW] =      "low",
	[LOW_MEMORY_LEVEL_MEDIUM] =   "medium",
	[LOW_MEMORY_LEVEL_CRITICAL] = "critical"
    };

    if ((int) level < LOW_MEMORY_LEVEL_INVALID ||
	(int) level >= LOW_MEMORY_LEVEL_COUNT) {
	g_assert_not_reached ();
    }

    if (level == LOW_MEMORY_LEVEL_INVALID)
	return "unset";
    return str[level];
}

static guint8
level_enum_to_byte (LowMemoryLevelEnum level)
{
    guint8 level_bytes[LOW_MEMORY_LEVEL_COUNT] = {
        [LOW_MEMORY_LEVEL_LOW] =       50,
        [LOW_MEMORY_LEVEL_MEDIUM] =   100,
        [LOW_MEMORY_LEVEL_CRITICAL] = 255
    };

    if ((int) level < LOW_MEMORY_LEVEL_INVALID ||
	(int) level >= LOW_MEMORY_LEVEL_COUNT) {
	g_assert_not_reached ();
    }

    if (level == LOW_MEMORY_LEVEL_INVALID)
        return 0;
    return level_bytes[level];
}

static float
mem_available (void)
{
    int fd;
    char buf[2048];
    char *p;
    guint64 mem_total, mem_available;

    fd = open ("/proc/meminfo", O_RDONLY);
    if (fd < 0)
         return -1;
    if (read(fd, buf, sizeof(buf)) < 0) {
	close (fd);
	return -1;
    }
    buf[sizeof(buf) - 1] = '\0';
    close (fd);

    p = strstr (buf, "MemTotal:");
    if (p == NULL)
	return -1;

    mem_total = g_ascii_strtoull (p, &p, 10);
    if (mem_total == 0)
	return -1;

    /* MemAvailable should be after MemTotal */
    p = strstr (p, "MemAvailable:");
    if (p == NULL)
	return -1;

    mem_available = g_ascii_strtoull (p, NULL, 10);

    return mem_available / mem_total;
}

static LowMemoryLevelEnum
find_level (LowMemoryMonitorData *data,
	    guint                 source_id)
{
    guint i;

    for (i = 0; i < LOW_MEMORY_LEVEL_COUNT; i++) {
	if (source_id == data->source_id[i])
	    return i;
    }

    g_assert_not_reached ();
}

static gboolean
emit_signal (LowMemoryMonitorData *data,
	     LowMemoryLevelEnum    level)
{
    gboolean ret = TRUE;

    g_dbus_connection_emit_signal (data->connection,
				   NULL,
				   LOW_MEMORY_MONITOR_DBUS_PATH,
				   LOW_MEMORY_MONITOR_IFACE_NAME,
				   "LowMemoryWarning",
				   g_variant_new ("(y)", level_enum_to_byte (level)),
				   NULL);
    data->current_state = level;
    data->next_state = LOW_MEMORY_LEVEL_INVALID;

    if (level == LOW_MEMORY_LEVEL_CRITICAL) {
	if (data->trigger_kernel_oom) {
	    GError *error = NULL;
	    if (!sysrq_trigger_oom (&error)) {
		g_warning ("Failed to trigger OOM: %s", error->message);
		g_error_free (error);
		ret = FALSE;
	    }
	} else {
	    g_debug ("Would trigger OOM, but disabled in configuration");
	}
    }

    return ret;
}

static gboolean
emit_signal_deferred (gpointer user_data)
{
    LowMemoryMonitorData *data = user_data;

    if (data->next_state < data->current_state) {
	g_debug ("Not emitting deferred signal for %s, current state is %s",
		 levels_str(data->next_state), levels_str(data->current_state));
	goto out;
    }

    emit_signal (data, data->next_state);

out:
    data->signal_timeout = 0;
    return G_SOURCE_REMOVE;
}

static void
cancel_handle_level_deferred (LowMemoryMonitorData *data)
{
    if (data->signal_timeout > 0) {
	g_source_remove (data->signal_timeout);
	data->signal_timeout = 0;
    }
}

static gboolean
handle_level_deferred (LowMemoryMonitorData *data,
		       LowMemoryLevelEnum    next_state,
		       gint64                current_time)
{
    if (next_state <= data->current_state ||
	next_state == LOW_MEMORY_LEVEL_CRITICAL) {
	g_debug ("Not deferring state change, next state is %s (current state: %s)",
		 levels_str(next_state), levels_str(data->current_state));
	cancel_handle_level_deferred (data);
	return FALSE;
    }

    data->next_state = next_state;
    data->last_trigger[data->next_state] = current_time;
    cancel_handle_level_deferred (data);

    g_debug ("Switching to state %s shortly", levels_str(next_state));
    data->signal_timeout = g_idle_add (emit_signal_deferred, data);

    return TRUE;
}

static gboolean
memory_pressure_cb (guint    source_id,
		    gpointer user_data)
{
    LowMemoryMonitorData *data = user_data;
    LowMemoryLevelEnum level;
    gint64 current_time;
    float mem_ratio;

    level = find_level (data, source_id);
    g_debug ("Received memory pressure callback for %s", levels_str(level));

    current_time = g_get_monotonic_time ();

    mem_ratio = mem_available ();
    if (mem_ratio >= 0.5) {
	g_debug ("Available memory is at %d%%, not sending %s signal",
		 (int) (mem_ratio * 100),
		 levels_str(level));
	return G_SOURCE_CONTINUE;
    }

    if (handle_level_deferred (data, level, current_time))
	return G_SOURCE_CONTINUE;

    if (data->last_trigger[level] == 0 ||
	(current_time - data->last_trigger[level]) > (RECOVERY_INTERVAL * G_USEC_PER_SEC)) {
	G_DEBUG_HERE();

	if (emit_signal (data, level))
	    data->last_trigger[level] = current_time;
    }
    return G_SOURCE_CONTINUE;
}

static void
name_lost_handler (GDBusConnection *connection,
                   const gchar     *name,
                   gpointer         user_data)
{
    g_debug ("low-memory-monitor is already running, or it cannot own its D-Bus name. Verify installation.");
    exit (0);
}

static const GDBusInterfaceVTable interface_vtable =
{
    NULL,
    NULL,
    NULL
};

static void
bus_acquired_handler (GDBusConnection *connection,
                      const gchar     *name,
                      gpointer         user_data)
{
    LowMemoryMonitorData *data = user_data;

    g_dbus_connection_register_object (connection,
				       LOW_MEMORY_MONITOR_DBUS_PATH,
				       data->introspection_data->interfaces[0],
				       &interface_vtable,
				       data,
				       NULL,
				       NULL);

    data->connection = g_object_ref (connection);
}

static void
name_acquired_handler (GDBusConnection *connection,
		       const gchar     *name,
		       gpointer         user_data)
{
    LowMemoryMonitorData *data = user_data;
    GError *error = NULL;
    guint i;

    for (i = 0; i < G_N_ELEMENTS(triggers); i++) {
	data->source_id[i] = memory_pressure_monitor_add_trigger (triggers[i].trigger_type,
								  triggers[i].threshold_ms * 1000,
								  POLL_INTERVAL * 1000 * 1000,
								  memory_pressure_cb,
								  data,
								  &error);
	if (data->source_id[i] == 0) {
	    g_warning ("Failed to add memory pressure monitor for %s: %s",
		       levels_str(i), error->message);
	    g_error_free (error);
	    goto bail;
	}
    }

    return;

bail:
    data->ret = 0;
    g_main_loop_quit (data->loop);
}

static void
setup_dbus (LowMemoryMonitorData *data)
{
    GBytes *bytes;

    bytes = g_resources_lookup_data ("/org/freedesktop/LowMemoryMonitor/org.freedesktop.LowMemoryMonitor.xml",
				     G_RESOURCE_LOOKUP_FLAGS_NONE,
				     NULL);
    data->introspection_data = g_dbus_node_info_new_for_xml (g_bytes_get_data (bytes, NULL), NULL);
    g_bytes_unref (bytes);
    g_assert (data->introspection_data != NULL);

    data->name_id = g_bus_own_name (G_BUS_TYPE_SYSTEM,
				    LOW_MEMORY_MONITOR_DBUS_NAME,
				    G_BUS_NAME_OWNER_FLAGS_NONE,
				    bus_acquired_handler,
				    name_acquired_handler,
				    name_lost_handler,
				    data,
				    NULL);
}

static void
free_monitor_data (LowMemoryMonitorData *data)
{
    if (data == NULL)
	return;

    if (data->name_id != 0) {
	g_bus_unown_name (data->name_id);
	data->name_id = 0;
    }

    g_clear_pointer (&data->introspection_data, g_dbus_node_info_unref);
    g_clear_object (&data->connection);
    g_clear_pointer (&data->loop, g_main_loop_unref);
    g_free (data);
}

static void
read_configuration (LowMemoryMonitorData *data)
{
    g_autoptr(GKeyFile) keyfile = NULL;
    g_autoptr(GError) error = NULL;

    data->trigger_kernel_oom = TRIGGER_KERNEL_OOM;
    keyfile = g_key_file_new ();
    if (!g_key_file_load_from_file (keyfile,
				    SYSCONFDIR "/low-memory-monitor.conf",
				    G_KEY_FILE_NONE,
				    &error)) {
	g_debug ("Could not read configuration file (%s), using TriggerKernelOom configuration %s",
		 error->message,
		 data->trigger_kernel_oom ? "'true'" : "'false'");
	return;
    }

    data->trigger_kernel_oom = g_key_file_get_boolean (keyfile,
						       "Configuration",
						       "TriggerKernelOom",
						       &error);
    if (!data->trigger_kernel_oom &&
	error != NULL) {
	data->trigger_kernel_oom = TRIGGER_KERNEL_OOM;
	g_warning ("Could not read configuration entry TriggerKernelOom (%s), using default %s",
		   error->message,
		   data->trigger_kernel_oom ? "'true'" : "'false'");
    }
}

int
main (int argc, char **argv)
{
    LowMemoryMonitorData *data;
    GError *error = NULL;
    int ret;

    if (!lock_memory (&error)) {
        g_warning ("Failed to lock memory: %s", error->message);
        g_error_free (error);
        return 1;
    }

    data = g_new0 (LowMemoryMonitorData, 1);
    data->current_state = LOW_MEMORY_LEVEL_INVALID;
    data->next_state = LOW_MEMORY_LEVEL_INVALID;
    read_configuration (data);
    setup_dbus (data);

    data->loop = g_main_loop_new (NULL, TRUE);
    g_main_loop_run (data->loop);

    ret = data->ret;
    free_monitor_data (data);

    return ret;
}

/*
 * vim: sw=4 ts=8 cindent noai bs=2
 */
