/*
 * Copyright (c) 2019 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include <lock-memory.h>

#include <gio/gio.h>
#include <sys/mman.h>
#include <errno.h>

/* Adapted from:
 * https://android.googlesource.com/platform/system/core/+/master/lmkd/lmkd.c
 */

gboolean
lock_memory (GError **error)
{
    /*
     * MCL_ONFAULT pins pages as they fault instead of loading
     * everything immediately all at once. (Which would be bad,
     * because as of this writing, we have a lot of mapped pages we
     * never use.) Old kernels will see MCL_ONFAULT and fail with
     * EINVAL; we ignore this failure.
     *
     * N.B. read the man page for mlockall. MCL_CURRENT | MCL_ONFAULT
     * pins ⊆ MCL_CURRENT, converging to just MCL_CURRENT as we fault
     * in pages.
     */
    /* CAP_IPC_LOCK required */
    if (mlockall(MCL_CURRENT | MCL_FUTURE | MCL_ONFAULT) && (errno != EINVAL)) {
        g_set_error (error,
		     G_IO_ERROR,
		     g_io_error_from_errno (errno),
		     "mlockall failed: %m");
        return FALSE;
    }

    return TRUE;
}
