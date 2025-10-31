/*
 * Copyright (c) 2019 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#define _DEFAULT_SOURCE 1
#include <sys/mman.h>
#include <unistd.h>
#include <glib.h>

#define ONE_MB (1 << 20)

#define ALLOC_STEP (ONE_MB)
#define ALLOC_DELAY 1000

static volatile void *gptr;

static int delay_size = 0;
static int delay = 1;

static const GOptionEntry entries[] = {
        { "delay-size", 's',  0, G_OPTION_ARG_INT, &delay_size, "Stop and delay after this many MB are allocated (default is no delay)", NULL },
        { "delay", 'd',  0, G_OPTION_ARG_INT, &delay, "Length of the delay after allocating this many chunks (if there is a delay, default 1 sec)", NULL },
        { NULL }
};

int main (int argc, char **argv)
{
    size_t allocated_size = 0;
    volatile void *ptr;
    GOptionContext *context;
    GError *error = NULL;
    int allocated_chunks = 0;

    context = g_option_context_new ("Fill memory");
    g_option_context_add_main_entries (context, entries, NULL);

    if (g_option_context_parse (context, &argc, &argv, &error) == FALSE) {
	g_print ("couldn't parse command-line options: %s\n", error->message);
	g_error_free (error);
	return 1;
    }

    if (delay_size != 0) {
	g_print ("Will allocate %d chunks of %d MB, and then pause for %d seconds\n",
		 delay_size, ALLOC_STEP / ONE_MB,
		 delay);
    } else {
	g_print ("Will allocate in chunks of %d MB\n", ALLOC_STEP / ONE_MB);
    }

    g_print ("Legend:\n");
    g_print ("# allocation\n");
    g_print ("- pause\n");

    while (1) {
        ptr = mmap (NULL, ALLOC_STEP, PROT_READ | PROT_WRITE,
		    MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
        if (ptr != MAP_FAILED) {
            /* create ptr aliasing to prevent compiler optimizing the access */
            gptr = ptr;
            /* make data non-zero */
            memset((void*)ptr, (int)(allocated_size + 1), ALLOC_STEP);
            allocated_size += ALLOC_STEP;
	    allocated_chunks++;
	    g_print ("#");
        }
	if (delay_size != 0 &&
	    allocated_chunks == delay_size) {
	    g_debug ("Allocated %d chunks, sleeping for %d seconds\n",
		     allocated_chunks, delay);
	    g_print ("-");
	    sleep (delay);
	    allocated_chunks = 0;
	} else {
	    g_usleep(ALLOC_DELAY);
	}
    }

    return 0;
}

/*
 * vim: sw=4 ts=8 cindent noai bs=2
 */
