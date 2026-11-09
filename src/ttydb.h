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

#ifndef _TTYDB_H
#define _TTYDB_H

#include <time.h>

#include <glib.h>

#include "x11.h"

struct ttydb;

/* note that this enum is used as an array index */
enum term_type { TERM_XTERM = 0, TERM_RXVT, TERM_UNKNOWN };
#define TERM_URXVT			(TERM_RXVT)
#define N_TERM_TYPES			(TERM_UNKNOWN)

struct ttyinfo {
	char *tty_path;
	char *info_path;
	struct timespec info_ctime;
	x11_window_t window;
	enum term_type term_type;
};

struct ttydb_iter {
	struct ttydb *_db;
	struct ttyinfo *_cur;
	GHashTableIter _iter;
};

struct ttydb *ttydb_new(struct xcb_connection_t *);
void ttydb_free(struct ttydb *);
void ttydb_reload(struct ttydb *, struct xcb_connection_t *, const char *);
void ttydb_iter_start(struct ttydb *, struct ttydb_iter *);
const struct ttyinfo *ttydb_iter_next(struct ttydb_iter *);
void ttydb_invalidate_iter(struct ttydb_iter *, const char *);
void ttydb_invalidate_ttyinfo(struct ttydb *, struct ttyinfo *, const char *);
void ttydb_invalidate_window(struct ttydb *, x11_window_t, const char *);
void ttydb_validate_ttyinfo(struct ttydb *, struct ttyinfo *, const char *);
struct ttyinfo *ttydb_next_unvalidated(struct ttydb *, GList **);
const char *term_type_name(enum term_type);

#endif /*  _TTYDB_H */
