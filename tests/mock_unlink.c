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

#include <errno.h>
#include <string.h>

#include <glib.h>

#include "mock_unlink.h"

static gboolean is_setup;
static const struct mock_unlink_setup *params;
static size_t plen;
static GStrvBuilder *calls_b;
static char **calls_v;

void
mock_unlink_setup(const struct mock_unlink_setup *p, size_t len)
{
	g_assert_true(!is_setup);
	is_setup = TRUE;
	params = p;
	plen = len;
	calls_b = g_strv_builder_new();
	calls_v = NULL;
}

int
mock_unlink_func(const char *name)
{
	size_t i;

	g_assert_true(is_setup);
	g_strv_builder_add(calls_b, name);

	for (i = 0; i < plen; i++)
		if (params[i].filename != NULL &&
		    strcmp(params[i].filename, name) == 0) {
			errno = params[i].err;
			return params[i].result;
		}

	errno = ENOENT;
	return -1;
}

const char **
mock_unlink_get_calls(void)
{
	g_assert_true(is_setup);
	if (calls_v == NULL)
		calls_v = g_strv_builder_end(calls_b);
	return (const char **)calls_v;
}

void
mock_unlink_teardown(void)
{
	g_assert_true(is_setup);
	is_setup = FALSE;
	g_strv_builder_unref(calls_b);
	g_strfreev(calls_v);
}
