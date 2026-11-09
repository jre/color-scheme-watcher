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

#include <glib.h>
#include <gio/gio.h>

#include "color_scheme.h"
#include "log.h"
#include "settings.h"

#define GNOME_SCHEMA	"org.gnome.desktop.interface"
#define GNOME_KEY	"color-scheme"

struct settings {
	void (*changed_cb)(enum color_scheme, void*);
	void *cb_arg;
	enum color_scheme scheme;
	GSettings *gnome;
	gulong gnome_sig;
};

static enum color_scheme
gnome_get_scheme(GSettings *settings)
{
	switch (g_settings_get_enum(settings, GNOME_KEY)) {
	case 0: return SCHEME_DEFAULT;
	case 1: return SCHEME_DARK;
	case 2: return SCHEME_LIGHT;
	default: return SCHEME_UNKNOWN;
	}
}

static void
gnome_set_scheme(GSettings *settings, enum color_scheme scheme)
{
	gint value;

	switch (scheme) {
	case SCHEME_DEFAULT:
		value = 0;
		break;
	case SCHEME_DARK:
		value = 1;
		break;
	case SCHEME_LIGHT:
		value = 2;
		break;
	default:
		return;
	}
	log_debug("gnome settings set scheme %s (%d)",
	    color_scheme_name(scheme), value);
	g_settings_set_enum(settings, GNOME_KEY, value);
}

static void
gnome_on_changed(GSettings *settings, G_GNUC_UNUSED gchar *key,
    gpointer user_data)
{
	struct settings *impl = (struct settings *)user_data;
	enum color_scheme scheme;

	scheme = gnome_get_scheme(settings);
	log_info("gnome setting changed: %s", color_scheme_name(scheme));
	settings_set_scheme(impl, scheme);
}

struct settings *
settings_new(void (*changed)(enum color_scheme, void*), void *arg)
{
	GSettingsSchemaSource *source;
	GSettingsSchema *schema;
	struct settings *impl;
	enum color_scheme initial;

	impl = g_new0(struct settings, 1);
	impl->changed_cb = changed;
	impl->cb_arg = arg;
	impl->scheme = SCHEME_UNKNOWN;

	if ((source = g_settings_schema_source_get_default()) == NULL)
		return impl;

	schema = g_settings_schema_source_lookup(source, GNOME_SCHEMA, TRUE);
	if (schema != NULL) {
		impl->gnome = g_settings_new_full(schema, NULL, NULL);
		g_settings_schema_unref(schema);
		impl->gnome_sig = g_signal_connect(impl->gnome,
		    "changed::" GNOME_KEY, G_CALLBACK(gnome_on_changed), impl);
		initial = gnome_get_scheme(impl->gnome);
		log_debug("got gnome settings: %s",
		    color_scheme_name(initial));
		if (impl->scheme == SCHEME_UNKNOWN)
			impl->scheme = initial;
	}

	if (impl->scheme == SCHEME_UNKNOWN) {
		impl->scheme = SCHEME_DEFAULT;
		log_warn("no initial color scheme found, using default");
	} else
		log_info("found initial color scheme: %s",
		    color_scheme_name(impl->scheme));

	return impl;
}

void
settings_free(struct settings *impl)
{
	if (impl->gnome != NULL) {
		g_signal_handler_disconnect(impl->gnome, impl->gnome_sig);
		g_object_unref(impl->gnome);
	}

	g_free(impl);
}

enum color_scheme
settings_get_scheme(struct settings *impl)
{
	return impl->scheme;
}

void
settings_set_scheme(struct settings *impl, enum color_scheme scheme)
{
	log_debug("settings set scheme %s", color_scheme_name(scheme));

	/* Note that this function must be re-entrant:
	   settings_set_scheme()
	     at this point, scheme != impl->scheme
	     gnome_set_scheme()
	       at this point, enum value differs
	       g_settings_set_enum()
		 gnome_on_changed()
		   settings_set_scheme()
		     at this point, scheme != impl->scheme
		     gnome_set_scheme()
		       at this point, enum value is the same
		       g_settings_set_enum()
		     impl->scheme = scheme
		     impl->changed_cb()
	     at this point, scheme == impl->scheme
	*/
	if (impl->gnome != NULL)
		gnome_set_scheme(impl->gnome, scheme);

	if (scheme == impl->scheme)
		log_debug("ignoring unchanged scheme: %s",
		    color_scheme_name(scheme));
	else {
		log_info("scheme changed %s -> %s",
		    color_scheme_name(impl->scheme), color_scheme_name(scheme));
		impl->scheme = scheme;
		impl->changed_cb(scheme, impl->cb_arg);
	}
}
