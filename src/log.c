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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include "log.h"

static log_fatal_handler_t fatal_handler;
static void *fatal_data;
static FILE *logging_stream = stderr;
static enum log_mode logging_mode;
static gboolean verbose_mode;

#define NON_VERBOSE_MASK \
	(G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING)

static const char *
level_prefix(GLogLevelFlags level)
{
	static const char *prefix[N_LOG_MODES][7] = {
		[LOG_MODE_SYSTEMD] = {
			"<2>", "<3>", "<4>", "<5>", "<6>", "<7>"
		},
		[LOG_MODE_HUMAN] = {
			": FATAL: ", ": ERROR: ", ": WARNING: ",
			": NOTE: ", ": INFO: ", ": DEBUG: ",
		},
		[LOG_MODE_TAP] = {
			"# FATAL: ", "# ERROR: ", "# WARNING: ",
			"# NOTE: ", "# INFO: ", "# DEBUG: ",
		},
	};
	const char **strs = prefix[logging_mode];

	if (level & G_LOG_LEVEL_ERROR)
		return strs[0];
	else if (level & G_LOG_LEVEL_CRITICAL)
		return strs[1];
	else if (level & G_LOG_LEVEL_WARNING)
		return strs[2];
	else if (level & G_LOG_LEVEL_MESSAGE)
		return strs[3];
	else if (level & G_LOG_LEVEL_INFO)
		return strs[4];
	else
		return strs[5];
}

static gboolean
get_isotime(char *buf, size_t len, const char *format)
{
	struct tm tm;
	time_t now;

	now = time(NULL);
	if (localtime_r(&now, &tm) == NULL)
		return FALSE;
	if (strftime(buf, len, format, &tm) == 0)
		return FALSE;
	return TRUE;
}

static void
handler(const gchar *domain, GLogLevelFlags level, const gchar *msg,
    gpointer extra)
{
	static const char tmfmt[] = "%Y-%m-%dT%H:%M:%S%z: ";
	char tmbuf[] = "YYYY-MM-DDThh:mm:ss+hh:mm: ";
	char *domsuf, *suf1, *suf2, *suf3, *full;

	if (!log_enabled(level))
		return;

	if (domain == NULL)
		domain = domsuf = "";
	else
		domsuf = "";
	if (extra == NULL) {
		suf1 = "\n";
		suf2 = suf3 = NULL;
	} else {
		suf1 = ": ";
		suf2 = extra;
		suf3 = "\n";
	}

	if (logging_mode != LOG_MODE_HUMAN)
		full = g_strconcat(level_prefix(level),
		    domain, domsuf, msg, suf1, suf2, suf3, NULL);
	else if (verbose_mode && get_isotime(tmbuf, sizeof(tmbuf), tmfmt))
		full = g_strconcat(tmbuf, g_get_prgname(), level_prefix(level),
		    domain, domsuf, msg, suf1, suf2, suf3, NULL);
	else
		full = g_strconcat(g_get_prgname(), level_prefix(level),
		    domain, domsuf, msg, suf1, suf2, suf3, NULL);
	fputs(full, logging_stream);
	g_free(full);

	if (level & G_LOG_FLAG_FATAL) {
		if (fatal_handler == NULL)
			exit(1);
		else
			fatal_handler(1, fatal_data);
	}
}

static void
vhandler(const gchar *domain, GLogLevelFlags level, const gchar *fmt,
    va_list ap, gpointer extra) {
	char *msg;

	msg = g_strdup_vprintf(fmt, ap);
	handler(domain, level, msg, extra);
	g_free(msg);
}

void
logging_set_fatal_handler(log_fatal_handler_t fatal, void *data)
{
	fatal_handler = fatal;
	fatal_data = data;
}

void
logging_setup(enum log_mode mode)
{
	g_assert(logging_mode >= 0 && logging_mode < N_LOG_MODES);
	logging_mode = mode;
	if (mode == LOG_MODE_TAP)
		logging_stream = stdout;
	g_log_set_default_handler(handler, NULL);
}

void
logging_set_verbose(gboolean verbose)
{
	verbose_mode = verbose;
}

gboolean
log_enabled(GLogLevelFlags level)
{
	return logging_mode == LOG_MODE_SYSTEMD || verbose_mode ||
	    (level & NON_VERBOSE_MASK) != 0;
}

void
log_gvariant(const char *dom, GLogLevelFlags level, GVariant *value,
    const char *fmt, ...)
{
	gchar *printable;
	va_list ap;

	printable = g_variant_print(value, TRUE);
	va_start(ap, fmt);
	vhandler(dom, level, fmt, ap, printable);
	va_end(ap);
	g_free(printable);
}

void
log_gerror(const char *dom, GLogLevelFlags level, GError *err,
    const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vhandler(dom, level, fmt, ap, (err == NULL ? NULL : err->message));
	va_end(ap);
}

void
log_errno(const char *dom, GLogLevelFlags level, const char *fmt, ...)
{
	char buf[NL_TEXTMAX];
	va_list ap;
	int res;

	res = strerror_r(errno, buf, sizeof(buf));
	va_start(ap, fmt);
	vhandler(dom, level, fmt, ap, (res == 0 ? NULL : buf));
	va_end(ap);
}
