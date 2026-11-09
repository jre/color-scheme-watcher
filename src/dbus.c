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

#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include "dbus.h"
#include "log.h"
#include "names.h"

struct proxy_signal_data {
	dbus_proxy_func_t handler;
	gpointer user_data;
};

struct object_method_data {
	dbus_scheme_func_t get_scheme;
	gpointer user_data;
};

struct ownership_data {
	dbus_owned_func_t handler;
	gpointer user_data;
	gboolean owned;
};

static void
free_data(gpointer data, GClosure *closure G_GNUC_UNUSED)
{
	g_free(data);
}

GDBusConnection *
dbus_get(void)
{
	GDBusConnection *con;
	GError *err;

	err = NULL;
	if ((con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err)) == NULL)
		log_fatal_gerror(err, "failed to connect to session bus");
	g_dbus_connection_set_exit_on_close(con, TRUE);

	return con;
}

/*
  https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Settings.html
 */

static void
proxy_signal(GDBusProxy *proxy G_GNUC_UNUSED, char *s G_GNUC_UNUSED,
    gchar *signal, GVariant *params, gpointer user_data)
{
	struct proxy_signal_data *data = user_data;
	enum color_scheme newscheme;
	gchar *namespace, *key;
	GVariant *value;

	log_debug_gvariant(params, "proxy signal %s", signal);

	newscheme = SCHEME_UNKNOWN;
	if (strcmp(signal, CHANGED_SIG) != 0)
		log_warn("ignoring wrong signal: %s", signal);
	else if (!g_variant_check_format_string(params, "(ssv)", FALSE))
		log_error_gvariant(params, "bad signal param types");
	else {
		g_variant_get(params, "(&s&sv)", &namespace, &key, &value);
		if (strcmp(namespace, SETTINGS_NS) != 0 ||
		    strcmp(key, SETTINGS_KEY) != 0)
			log_error_gvariant(params, "bad signal param values");
		else
			newscheme = variant_color_scheme(value);
		g_variant_unref(value);
	}

	data->handler(newscheme, data->user_data);
}

GDBusProxy *
dbus_get_proxy_sync(GDBusConnection *con, gboolean autostart)
{
	GDBusProxyFlags flags;
	GDBusProxy *proxy;
	GError *err = NULL;

	flags = G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES;
	if (!autostart)
		flags |= G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START;
	if ((proxy = g_dbus_proxy_new_sync(con, flags, NULL, BUS_NAME_PUB,
		    OBJ_PATH, IFACE_NAME_PUB, NULL, &err)) == NULL)
		log_fatal_gerror(err, "failed to create dbus proxy");

	return proxy;
}

gulong
dbus_proxy_connect(GDBusProxy *proxy, dbus_proxy_func_t func,
    gpointer user_data)
{
	struct proxy_signal_data *data;

	data = g_new0(struct proxy_signal_data, 1);
	data->handler = func;
	data->user_data = user_data;;

	return g_signal_connect_data(proxy, "g-signal::" CHANGED_SIG,
	    G_CALLBACK(proxy_signal), data, free_data, G_CONNECT_DEFAULT);
}

enum color_scheme
dbus_proxy_scheme_sync(GDBusProxy *proxy)
{
	GError *err = NULL;
	GVariant *params, *res, *value;
	enum color_scheme scheme;

	params = g_variant_new("(ss)", SETTINGS_NS, "color-scheme");
	res = g_dbus_proxy_call_sync(proxy, "Read", params,
	    G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, &err);
	if (res == NULL) {
		log_error_gerror(err, "failed to read current value");
		return SCHEME_UNKNOWN;
	}
	log_debug_gvariant(res, "Read() returned");

	scheme = SCHEME_UNKNOWN;
	if (!g_variant_check_format_string(res, "(v)", FALSE))
		log_error_gvariant(res, "bad Read() return value");
	else {
		g_variant_get(res, "(v)", &value);
		scheme = variant_color_scheme(value);
		g_variant_unref(value);
	}
	g_variant_unref(res);

	return scheme;
}

/*
  https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.impl.portal.Settings.html
*/

static const char impl_interface_xml[] =
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<node name='" OBJ_PATH "'>\n"
    "    <interface name='" IFACE_NAME_IMPL "'>\n"
    "        <property name='version' access='read' type='u'/>\n"
    "        <method name='ReadAll'>\n"
    "            <arg direction='in' name='namespaces' type='as'/>\n"
    "            <arg direction='out' name='value' type='a{sa{sv}}'/>\n"
    "        </method>\n"
    "        <method name='Read'>\n"
    "            <arg direction='in' name='namespace' type='s'/>\n"
    "            <arg direction='in' name='key' type='s'/>\n"
    "            <arg direction='out' name='value' type='v'/>\n"
    "        </method>\n"
    "        <signal name='" CHANGED_SIG "'>\n"
    "            <arg name='namespace' type='s'/>\n"
    "            <arg name='key' type='s'/>\n"
    "            <arg name='value' type='v'/>\n"
    "        </signal>\n"
    "    </interface>\n"
    "</node>";

static GVariant *
method_call_read(GVariant *params, enum color_scheme scheme, GError **err)
{
	gchar *namespace, *key;
	GVariant *ret;

	if (!g_variant_check_format_string(params, "(ss)", FALSE)) {
		g_set_error(err, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
		    "invalid arguments");
		return NULL;
	}

	ret = NULL;
	g_variant_get(params, "(&s&s)", &namespace, &key);
	if (strcmp(namespace, SETTINGS_NS) == 0 &&
	    strcmp(key, SETTINGS_KEY) == 0)
		ret = g_variant_new("(v)", color_scheme_variant(scheme));
	else
		g_set_error(err, G_DBUS_ERROR, G_DBUS_ERROR_FILE_NOT_FOUND,
		    "unknown setting");

	return ret;
}

static GVariant *
method_call_read_all(GVariant *params, enum color_scheme scheme, GError **err)
{
	GVariantBuilder *outer, *inner;
	GVariantIter *iter;
	gboolean matched;
	gchar *query;

	if (!g_variant_check_format_string(params, "(as)", FALSE)) {
		g_set_error(err, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
		    "invalid arguments to ReadAll");
		return NULL;
	}

	g_variant_get(params, "(as)", &iter);
	matched = (g_variant_iter_n_children(iter) == 0);
	while (!matched && g_variant_iter_next(iter, "&s", &query)) {
		if (query[0] == '\0')
			matched = TRUE;
		else if (g_pattern_match_simple(query, SETTINGS_NS))
			matched = TRUE;
	}
	g_variant_iter_free(iter);

	outer = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));
	if (matched) {
		inner = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
		g_variant_builder_add(inner, "{sv}", SETTINGS_KEY,
		    color_scheme_variant(scheme));
		g_variant_builder_add(outer, "{sa{sv}}", SETTINGS_NS, inner);
	}
	return g_variant_new("(a{sa{sv}})", outer);
}

static void
method_call(GDBusConnection *con G_GNUC_UNUSED, const gchar *s G_GNUC_UNUSED,
    const gchar *path, const gchar *interface, const gchar *method,
    GVariant *params, GDBusMethodInvocation *inv, gpointer user_data)
{
	struct object_method_data *data = user_data;
	enum color_scheme scheme;
	GVariant *ret;
	GError *err;

	log_debug_gvariant(params, "method call %s/%s.%s",
	    path, interface, method);

	err = NULL;
	ret = NULL;
	scheme = data->get_scheme(data->user_data);
	if (strcmp(method, "Read") == 0)
		ret = method_call_read(params, scheme, &err);
	else if (strcmp(method, "ReadAll") == 0)
		ret = method_call_read_all(params, scheme, &err);
	else
		g_set_error(&err, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
		    "unknown method: %s", method);

	if (ret == NULL) {
		log_error_gerror(g_error_copy(err), "%s.%s() failed",
		    interface, method);
		g_dbus_method_invocation_take_error(inv, err);
	} else {
		log_debug_gvariant(ret, "%s.%s() returned", interface, method);
		g_dbus_method_invocation_return_value(inv, ret);
	}
}

static GVariant *
property_get(GDBusConnection *con G_GNUC_UNUSED, const gchar *s G_GNUC_UNUSED,
    const gchar *path, const gchar *interface, const gchar *property,
    GError **err, gpointer user_data G_GNUC_UNUSED)
{
	log_debug("object property get %s/%s.%s", path, interface, property);

	if (strcmp(property, "version") == 0)
		return g_variant_new_uint32(0);

	g_set_error(err, G_IO_ERROR, G_IO_ERROR_FAILED,
	    "invalid property: %s.%s", interface, property);
	return NULL;
}

static gboolean
property_set(GDBusConnection *con G_GNUC_UNUSED, const gchar *s G_GNUC_UNUSED,
    const gchar *path, const gchar *interface, const gchar *property,
    GVariant *value, GError **err, gpointer user_data G_GNUC_UNUSED)
{
	log_debug_gvariant(value, "object property set %s/%s.%s",
	    path, interface, property);

	g_set_error(err, G_IO_ERROR, G_IO_ERROR_FAILED,
	    "invalid property: %s.%s", interface, property);
	return FALSE;
}

guint
dbus_register_object(GDBusConnection *con, dbus_scheme_func_t func,
    gpointer user_data)
{
	GDBusInterfaceVTable vtable = {
		.method_call = method_call,
		.get_property = property_get,
		.set_property = property_set,
	};
	struct object_method_data *data;
	GDBusNodeInfo *node;
	GError *err;
	guint ret;

	err = NULL;
	node = g_dbus_node_info_new_for_xml(impl_interface_xml, &err);
	if (node == NULL)
		log_fatal_gerror(err, "failed to parse interface XML");

	data = g_new0(struct object_method_data, 1);
	data->get_scheme = func;
	data->user_data = user_data;

	err = NULL;
	ret = g_dbus_connection_register_object(con, OBJ_PATH,
	    node->interfaces[0], &vtable, data, g_free, &err);
	g_dbus_node_info_unref(node);
	if (ret == 0)
		log_fatal_gerror(err, "failed to register object");

	return ret;
}

static void
name_acquired(GDBusConnection *con G_GNUC_UNUSED, const gchar *name,
    gpointer user_data)
{
	struct ownership_data *data = user_data;
 
 	log_debug("bus name acquired: %s", name);
	data->owned = TRUE;
	data->handler(data->user_data);
}

static void
name_lost(GDBusConnection *con G_GNUC_UNUSED, const gchar *name,
    gpointer user_data)
{
	struct ownership_data *data = user_data;

	if (data->owned)
 		log_fatal("bus name lost: %s", name);
 	else
 		log_fatal("failed to acquire bus name: %s", name);
}

guint
dbus_own_name(GDBusConnection *con, gboolean replace, dbus_owned_func_t func,
    gpointer user_data)
{
	struct ownership_data *data;
	GBusNameOwnerFlags ownflags;

	data = g_new0(struct ownership_data, 1);
	data->handler = func;
	data->user_data = user_data;

	ownflags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;
	if (replace)
		ownflags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;
	return g_bus_own_name_on_connection(con, BUS_NAME_IMPL,
	    ownflags, name_acquired, name_lost, data, g_free);
}

gboolean
dbus_emit_scheme(GDBusConnection *con, enum color_scheme scheme)
{
	GError *err = NULL;
	GVariant *params;

	params = g_variant_new("(ssv)", SETTINGS_NS, SETTINGS_KEY,
	    color_scheme_variant(scheme));
	log_debug_gvariant(params, "emit %s %s.%s",
	    OBJ_PATH, IFACE_NAME_IMPL, CHANGED_SIG);
	if (g_dbus_connection_emit_signal(con, NULL, OBJ_PATH,
		IFACE_NAME_IMPL, CHANGED_SIG, params, &err))
		return TRUE;
	log_error_gerror(err, "failed to emit signal");
	return FALSE;
}
