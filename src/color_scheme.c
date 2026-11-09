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

#include "color_scheme.h"
#include "log.h"

enum color_scheme
variant_color_scheme(GVariant *value)
{
	GVariant *child;
	guint32 intval;

	g_variant_ref(value);
	while (g_variant_type_is_variant(g_variant_get_type(value))) {
		child = g_variant_get_variant(value);
		g_variant_unref(value);
		value = child;
	}

	if (!g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32)) {
		log_error("expected uint32 but found %s",
		    g_variant_get_type_string(value));
		return SCHEME_UNKNOWN;
	}
	switch ((intval = g_variant_get_uint32(value))) {
		case SCHEME_DEFAULT:
		case SCHEME_DARK:
		case SCHEME_LIGHT:
			return intval;
	default:
		log_warn("unknown scheme value: %u", intval);
		return SCHEME_UNKNOWN;
	}
}

GVariant *color_scheme_variant(enum color_scheme scheme)
{
	switch (scheme) {
	case SCHEME_DEFAULT:
	case SCHEME_DARK:
	case SCHEME_LIGHT:
		break;
	default:
		scheme = SCHEME_DEFAULT;
		break;
	}
	return g_variant_new_uint32(scheme);
}
	
const char *
color_scheme_name(enum color_scheme scheme)
{
	switch (scheme) {
	case SCHEME_DEFAULT:
		return "default";
	case SCHEME_DARK:
		return "dark";
	case SCHEME_LIGHT:
		return "light";
	default:
		return "unknown";
	}
}

enum color_scheme
color_scheme_parse(const char *str)
{
	if (strcasecmp(str, "default") == 0)
		return SCHEME_DEFAULT;
	else if (strcasecmp(str, "dark") == 0)
		return SCHEME_DARK;
	else if (strcasecmp(str, "light") == 0)
		return SCHEME_LIGHT;
	else
		return SCHEME_UNKNOWN;
}
