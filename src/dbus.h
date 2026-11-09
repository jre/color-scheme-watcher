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

#ifndef _DBUS_H
#define _DBUS_H

#include <glib.h>
#include <gio/gio.h>

#include "color_scheme.h"

typedef void (*dbus_proxy_func_t)(enum color_scheme, gpointer);
typedef enum color_scheme (*dbus_scheme_func_t)(gpointer);
typedef void (*dbus_owned_func_t)(gpointer);

GDBusConnection *dbus_get(void);
GDBusProxy *dbus_get_proxy_sync(GDBusConnection *, gboolean);
gulong dbus_proxy_connect(GDBusProxy *, dbus_proxy_func_t, gpointer);
enum color_scheme dbus_proxy_scheme_sync(GDBusProxy *);
guint dbus_register_object(GDBusConnection *, dbus_scheme_func_t, gpointer);
guint dbus_own_name(GDBusConnection *, gboolean, dbus_owned_func_t, gpointer);
gboolean dbus_emit_scheme(GDBusConnection *, enum color_scheme);

#endif /* _DBUS_H */
