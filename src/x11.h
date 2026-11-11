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

#ifndef _X11UTIL_H
#define _X11UTIL_H

#include <stdint.h>

#include <glib.h>

#include "escapes.h"

struct config_pair;
struct xcb_connection_t;
typedef uint32_t x11_window_t;

struct x11_term_info {
	const char *tty_path;
	enum term_type term_type;
};

struct xcb_connection_t *x11_connect(void);
void x11_disconnect(struct xcb_connection_t *);
GList *x11_find_term_windows(struct xcb_connection_t *);
void x11_free_term_windows(GList *);
void x11_update_resources(struct xcb_connection_t *,
    const struct config_pair *, size_t);

#endif /* _X11UTIL_H */
