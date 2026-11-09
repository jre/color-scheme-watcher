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

#include "config.h"
#include "escapes.h"
#include "ttydb.h"
#include "util.h"

/*  https://www.xfree86.org/current/ctlseqs.html */

/*
  XTerm Operating System Controls Set Text Parameter sequence:
    ESC ] <number> ; <param> BEL
*/
#define PRE			"\x1b]"
#define SEP			";"
#define SUF			"\x07"

#define PRE_ANSI_COLOR_SINGLE	PRE "4" SEP
#define PRE_ANSI_COLOR_MULTI	PRE "4"
#define PRE_TEXT_FG		PRE "10" SEP
#define PRE_TEXT_BG		PRE "11" SEP
#define PRE_CURSOR		PRE "12" SEP
#define PRE_MOUSE_FG		PRE "13" SEP
#define PRE_MOUSE_BG		PRE "14" SEP
#define PRE_HILITE_FG		PRE "17" SEP
#define PRE_HILITE_BG		PRE "19" SEP

static char *pre_colors_single[CONFIG_NCOLORS];
static char *pre_colors_multi[CONFIG_NCOLORS];

static void
init(void)
{
	unsigned int i;

	if (pre_colors_single[0] != NULL)
		return;

	for (i = 0; i < CONFIG_NCOLORS; i++) {
		pre_colors_single[i] = g_strdup_printf("%s%u%s",
		    PRE_ANSI_COLOR_SINGLE, i, SEP);
		pre_colors_multi[i] = g_strdup_printf("%s%u%s",
		    SEP, i, SEP);
	}
}

static char *
get_single(const struct config_scheme *scheme)
{
	struct { const char *pre; const char *color; } kv[] = {
		{ PRE_TEXT_FG, scheme->foreground },
		{ PRE_TEXT_BG, scheme->background },
		{ PRE_CURSOR, scheme->cursor },
		{ PRE_MOUSE_FG, scheme->mouse_foreground },
		{ PRE_MOUSE_BG, scheme->mouse_background },
		{ PRE_HILITE_FG, scheme->highlight_foreground },
		{ PRE_HILITE_BG, scheme->highlight_background },
	};
	const char *parts[NITEMS(kv) * 3 + CONFIG_NCOLORS * 3 + 1];
	unsigned int i, off;

	off = 0;
	for (i = 0; i < NITEMS(kv); i++) {
		if (kv[i].color == NULL)
			continue;
		parts[off++] = kv[i].pre;
		parts[off++] = kv[i].color;
		parts[off++] = SUF;
	}
	for (i = 0; i < CONFIG_NCOLORS; i++) {
		if (scheme->color[i] == NULL)
			continue;
		parts[off++] = pre_colors_single[i];
		parts[off++] = scheme->color[i];
		parts[off++] = SUF;
	}
	parts[off] = NULL;

	return g_strjoinv("", (char **)parts);
}

static char *
get_multi(const struct config_scheme *scheme)
{
	struct { const char *pre; const char *color; } kv[] = {
		{ PRE_TEXT_FG, /* 10 */ scheme->foreground },
		{ SEP, /* 11 */ scheme->background },
		{ SEP, /* 12 */ scheme->cursor },
		{ SEP, /* 13 */ scheme->mouse_foreground },
		{ SEP, /* 14 */ scheme->mouse_background },
		{ SEP /* 15 */ SEP /* 16 */
		  SEP, /* 17 */ scheme->highlight_foreground },
		{ SEP /* 18 */ SEP, /* 19 */ scheme->highlight_background },
	};
	const char *parts[NITEMS(kv) * 2 + 1 + CONFIG_NCOLORS * 2 + 1 + 1];
	unsigned int i, off;

	off = 0;
	for (i = 0; i < NITEMS(kv); i++) {
		parts[off++] = kv[i].pre;
		parts[off++] = (kv[i].color ? kv[i].color : "");
	}
	parts[off++] = SUF PRE_ANSI_COLOR_MULTI;
	for (i = 0; i < CONFIG_NCOLORS; i++) {
		if (scheme->color[i] == NULL)
			continue;
		parts[off++] = pre_colors_multi[i];
		parts[off++] = scheme->color[i];
	}
	parts[off++] = SUF;
	parts[off] = NULL;

	return g_strjoinv("", (char **)parts);
}

char *
get_term_escapes(enum term_type term, const struct config_scheme *scheme)
{
	init();

	switch (term) {
	case TERM_XTERM:
		return get_multi(scheme);
	case TERM_RXVT:
		return get_single(scheme);
	default:
		return NULL;
	}
}
