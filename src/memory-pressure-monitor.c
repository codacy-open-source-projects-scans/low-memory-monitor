/*
 * Copyright (c) 2019 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#define _GNU_SOURCE
#include <memory-pressure-monitor.h>
#include <glib-unix.h>
#include <errno.h>

/* Constants */
#define MEMORY_PRESSURE_PATH "/proc/pressure/memory"

typedef struct {
	int fd;
	guint source_id;
	MemoryPressureMonitorSourceFunc func;
	gpointer user_data;
} MemoryPressureMonitorData;

static const char *trigger_type_names[] = {
	"some",
	"full"
};

static void
mpm_data_free (MemoryPressureMonitorData *data)
{
	g_return_if_fail (data != NULL);
	close (data->fd);
	g_free (data);
}

static gboolean
mpm_fd_event (int          fd,
	      GIOCondition condition,
	      gpointer     user_data)
{
	MemoryPressureMonitorData *data = user_data;

	if (condition & G_IO_ERR)
		return FALSE;
	return (* data->func) (data->source_id, data->user_data);
}

guint
memory_pressure_monitor_add_trigger (MemoryPressureMonitorTriggerType   trigger_type,
				     int                                threshold_us,
				     int                                window_us,
				     MemoryPressureMonitorSourceFunc    func,
				     gpointer                           user_data,
				     GError                           **error)
{
	int fd, res;
	MemoryPressureMonitorData *data;
	g_autofree char *trigger = NULL;

	g_return_val_if_fail (func != NULL, 0);
	g_return_val_if_fail (trigger_type == MEMORY_PRESSURE_MONITOR_TRIGGER_SOME ||
			      trigger_type == MEMORY_PRESSURE_MONITOR_TRIGGER_FULL, 0);

	fd = TEMP_FAILURE_RETRY(open(MEMORY_PRESSURE_PATH, O_RDWR | O_NONBLOCK));
	if (fd < 0) {
		g_set_error (error,
			     G_UNIX_ERROR, 0,
			     "Could not open %s: %s",
			     MEMORY_PRESSURE_PATH,
			     g_strerror (errno));
		return 0;
	}

	trigger = g_strdup_printf ("%s %d %d",
				   trigger_type_names[trigger_type],
				   threshold_us, window_us);

	res = TEMP_FAILURE_RETRY(write (fd, trigger, strlen(trigger) + 1));
	if (res < 0) {
		g_set_error (error,
			     G_UNIX_ERROR, 0,
			     "Could not write trigger to %s: %s",
			     MEMORY_PRESSURE_PATH,
			     g_strerror (errno));
		close (fd);
		return 0;
	}

	data = g_new0 (MemoryPressureMonitorData, 1);
	data->fd = fd;
	data->func = func;
	data->user_data = user_data;

	data->source_id = g_unix_fd_add_full (G_PRIORITY_DEFAULT,
					      fd,
					      G_IO_PRI | G_IO_ERR,
					      mpm_fd_event,
					      data,
					      (GDestroyNotify) mpm_data_free);

	return data->source_id;
}
