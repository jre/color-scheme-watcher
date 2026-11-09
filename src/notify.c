/*
 * Copyright (c) 2026 Joshua R. Elsasser.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <inttypes.h>
#include <stdint.h>
#include <time.h>

#include <glib.h>
#include <gio/gio.h>

#include "log.h"

#define timespec_usecs(ts) \
	((((uint64_t)(ts)->tv_sec) * UINT64_C(1000000)) + \
	 (((uint64_t)(ts)->tv_nsec) / UINT64_C(1000)))

/* https://www.freedesktop.org/software/systemd/man/latest/sd_notify.html */

static GSocket *notify_sock;

static void
sock_send(const char *message)
{
	GError *err = NULL;

	if (g_socket_send_with_blocking(notify_sock, message, strlen(message),
		TRUE, NULL, &err) == -1)
		log_error_gerror(err, "failed to write to NOTIFY_SOCKET");
}

void
notify_starting(void)
{
	GError *err = NULL;
	GSocketAddress *addr;
	const char *path;
	GSocket *sock;

	path = g_getenv("NOTIFY_SOCKET");
	logging_setup(path ? LOG_MODE_SYSTEMD : LOG_MODE_HUMAN);
	if (path == NULL)
		return;

	addr = NULL;
	switch (path[0]) {
	case '/':
		addr = g_unix_socket_address_new(path);
		break;
	case '@':
		addr = g_unix_socket_address_new_with_type(path, -1,
		    G_UNIX_SOCKET_ADDRESS_ABSTRACT);
		break;
	default:
		log_error("unsupported NOTIFY_SOCKET value: %s", path);
		break;
	}

	g_unsetenv("NOTIFY_SOCKET");

	if (addr == NULL)
		return;
	if ((sock = g_socket_new(G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_DATAGRAM,
		    0, &err)) == NULL)
		log_error_gerror(err, "failed to create socket");
	else if (!g_socket_connect(sock, addr, NULL, &err)) {
		log_error_gerror(err, "failed to connect socket");
		g_object_unref(sock);
	} else
		notify_sock = sock;

	g_object_unref(addr);
}

void
notify_ready(void)
{
	if (notify_sock != NULL)
		sock_send("READY=1");
}

void
notify_reloading(void)
{
	struct timespec ts;

	if (notify_sock == NULL)
		return;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		sock_send("RELOADING=1");
	else {
		char *msg = g_strdup_printf( "RELOADING=1\n"
		    "MONOTONIC_USEC=%" PRIu64, timespec_usecs(&ts));
		sock_send(msg);
		g_free(msg);
	}
}

void
notify_stopping(void)
{
	if (notify_sock != NULL)
		sock_send("STOPPING=1");
}

void
notify_status(const char *format, ...)
{
	char *msg1, *msg2;
	va_list ap;

	if (notify_sock == NULL)
		return;

	va_start(ap, format);
	msg1 = g_strdup_vprintf(format, ap);
	va_end(ap);
	msg2 = g_strconcat("STATUS=", msg1, NULL);
	g_free(msg1);
	sock_send(msg2);
	g_free(msg2);
}
