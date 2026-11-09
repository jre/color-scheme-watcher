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

#ifndef _LOG_H
#define _LOG_H

#include <glib.h>

typedef void (*log_fatal_handler_t)(int, void *);

/* note that this enum is used as an array index */
enum log_mode {
	LOG_MODE_HUMAN = 0, LOG_MODE_SYSTEMD, LOG_MODE_TAP,
	N_LOG_MODES
};

void logging_set_fatal_handler(log_fatal_handler_t, void *);
void logging_setup(enum log_mode);
void logging_set_verbose(gboolean);

#define log_debug_enabled()	log_enabled(G_LOG_LEVEL_DEBUG)
#define log_debug(...) \
	(g_log(NULL, G_LOG_LEVEL_DEBUG, __VA_ARGS__))
#define log_debug_gvariant(...) \
	(log_gvariant(NULL, G_LOG_LEVEL_DEBUG, __VA_ARGS__))

#define log_info_enabled()	log_enabled(G_LOG_LEVEL_INFO)
#define log_info(...) \
	(g_log(NULL, G_LOG_LEVEL_INFO, __VA_ARGS__))
#define log_info_gvariant(...) \
	(log_gvariant(NULL, G_LOG_LEVEL_INFO, __VA_ARGS__))

#define log_note_enabled()	log_enabled(G_LOG_LEVEL_NOTE)
#define log_note(...) \
	(g_log(NULL, G_LOG_LEVEL_MESSAGE, __VA_ARGS__))
#define log_note_gvariant(...) \
	(log_gvariant(NULL, G_LOG_LEVEL_MESSAGE, __VA_ARGS__))

#define log_warn_enabled()	log_enabled(G_LOG_LEVEL_WARN)
#define log_warn(...) \
	(g_log(NULL, G_LOG_LEVEL_WARNING, __VA_ARGS__))
#define log_warn_gvariant(...) \
	(log_gvariant(NULL, G_LOG_LEVEL_WARNING, __VA_ARGS__))
#define log_warn_gerror(...) \
	(log_gerror(NULL, G_LOG_LEVEL_WARNING, __VA_ARGS__))

#define log_error_enabled()	log_enabled(G_LOG_LEVEL_ERROR)
#define log_error(...) \
	(g_log(NULL, G_LOG_LEVEL_CRITICAL, __VA_ARGS__))
#define log_error_gvariant(...) \
	(log_gvariant(NULL, G_LOG_LEVEL_CRITICAL, __VA_ARGS__))
#define log_error_gerror(...) \
	(log_gerror(NULL, G_LOG_LEVEL_CRITICAL, __VA_ARGS__))
#define log_error_errno(...) \
	(log_errno(NULL, G_LOG_LEVEL_CRITICAL, __VA_ARGS__))

#define log_fatal_enabled()	log_enabled(G_LOG_LEVEL_FATAL)
#define log_fatal(...) \
	(g_log(NULL, G_LOG_LEVEL_ERROR|G_LOG_FLAG_FATAL, __VA_ARGS__))
#define log_fatal_gerror(...) \
	(log_gerror(NULL, G_LOG_LEVEL_ERROR|G_LOG_FLAG_FATAL, __VA_ARGS__))
#define log_fatal_errno(...) \
	(log_errno(NULL, G_LOG_LEVEL_ERROR|G_LOG_FLAG_FATAL, __VA_ARGS__))

gboolean log_enabled(GLogLevelFlags);
void log_gvariant(const char *, GLogLevelFlags, GVariant *, const char *, ...)
	G_GNUC_PRINTF(4, 5);
void log_gerror(const char *, GLogLevelFlags, GError *, const char *, ...)
	G_GNUC_PRINTF(4, 5);
void log_errno(const char *, GLogLevelFlags, const char *, ...)
	G_GNUC_PRINTF(3, 4);

#endif /* _LOG_H */
