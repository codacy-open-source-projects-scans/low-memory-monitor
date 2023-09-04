/*
 * Copyright (C) 2018 Endless Mobile, Inc.
 * Copyright (c) 2019 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include <sysrq-oom.h>

#include <gio/gio.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define SYSRQ_TRIGGER_FILE  "/proc/sysrq-trigger"
#define SYSRQ_ALL_ON	    0x01
#define SYSRQ_MASK          0x40
#define BUFSIZE             256

static ssize_t
fstr (const char  *path,
      char        *rbuf,
      const char  *wbuf,
      GError     **error)
{
    int fd;
    ssize_t n;

    g_return_val_if_fail ((!rbuf && wbuf) || (rbuf && !wbuf), -1);

    fd = open(path, rbuf ? O_RDONLY : O_WRONLY);
    if (fd < 0) {
        g_set_error (error,
		     G_IO_ERROR,
		     g_io_error_from_errno (errno),
		     "Opening %s failed: %s", path, g_strerror (errno));
        return -1;
    }

    if (rbuf)
        n = read(fd, rbuf, BUFSIZE);
    else
        n = write(fd, wbuf, strlen(wbuf));
    if (n < 0) {
        g_set_error (error,
		     G_IO_ERROR,
		     g_io_error_from_errno (errno),
		     "Opening %s failed: %s", path, g_strerror (errno));
	close (fd);
        return -1;
    }
    close(fd);

    if (rbuf)
        rbuf[n-1] = '\0';

    return n;
}

gboolean
sysrq_trigger_oom (GError **error)
{
    g_debug ("Above threshold limit, killing task and pausing for recovery");
    if (fstr (SYSRQ_TRIGGER_FILE, NULL, "f", error) < 0)
	return FALSE;
    return TRUE;
}

/*
 * vim: sw=4 ts=8 cindent noai bs=2
 */
