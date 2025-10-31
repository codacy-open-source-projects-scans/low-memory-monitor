/*
 * Copyright (c) 2019 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include <glib.h>

typedef enum {
	MEMORY_PRESSURE_MONITOR_TRIGGER_SOME,
	MEMORY_PRESSURE_MONITOR_TRIGGER_FULL
} MemoryPressureMonitorTriggerType;

typedef gboolean (*MemoryPressureMonitorSourceFunc) (guint    source_id,
						     gpointer user_data);

guint memory_pressure_monitor_add_trigger (MemoryPressureMonitorTriggerType   trigger_type,
					   int                                threshold_us,
					   int                                window_us,
					   MemoryPressureMonitorSourceFunc    func,
					   gpointer                           user_data,
					   GError                           **error);
