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

#ifndef _PARSER_H
#define _PARSER_H

/* #define PARSE_GRAMMAR_DBG */

#include <stddef.h>
#include <stdio.h>

#include "color_scheme.h"
#include "datetime.h"
#include "scheduler.h"

enum parse_date_fmt { PARSE_DATE_FMT_DM, PARSE_DATE_FMT_MD };
enum parse_ampm { PARSE_AM, PARSE_PM, PARSE_24H };

struct parse_state {
	const char *filename;
	FILE *stream;
	enum parse_date_fmt date_fmt;
	ssize_t last_line, last_col;
	gboolean fail;
};

struct parse_node {
	enum parse_node_type {
		PARSE_NODE_NONE = 0,
		PARSE_NODE_SCHEME,	/* scheme */
		PARSE_NODE_UINT,	/* uint */
		PARSE_NODE_TOD,		/* tod, time of day in secs */
		PARSE_NODE_WDAY,	/* wday */
		PARSE_NODE_MDAY,	/* wday */
		PARSE_NODE_MONTH,	/* month */
		PARSE_NODE_DOY,		/* doy */
		PARSE_NODE_TODOY,	/* todoy */
		PARSE_NODE_TODOY_LIST,	/* todoy_list */
		PARSE_NODE_RULE,	/* rule */
	} type;
	union {
		enum color_scheme scheme;
		unsigned int uint;
		dt_tod_t tod;
		enum dt_wday wday;
		dt_mday_t mday;
		enum dt_month month;
		struct dt_monthday doy;
		struct dt_time_range_monthday todoy;
		struct dt_time_range_monthday_list todoy_list;
		struct sched_rule *rule;
	} u;
};

const struct sched_rule * const *parse_config(FILE *, const char *);
void parse_error(struct parse_state *, ssize_t, ssize_t);
#ifdef PARSE_GRAMMAR_DBG
void parse_dbg(int, const char *, unsigned int, size_t, const char *, int);
#endif

struct parse_node *parse_date_fmt(struct parse_state *, enum parse_date_fmt);
struct parse_node *parse_scheme(struct parse_state *, enum color_scheme);
struct parse_node *parse_uint_str(struct parse_state *, const char *);
struct parse_node *parse_tod(struct parse_state *,
    struct parse_node *, struct parse_node *, enum parse_ampm);
struct parse_node *parse_mday_str(struct parse_state *, const char *);
struct parse_node *parse_wday(struct parse_state *, enum dt_wday);
struct parse_node *parse_month(struct parse_state *, enum dt_month);
struct parse_node *parse_month_str(struct parse_state *, const char *);
struct parse_node *parse_doy(struct parse_state *,
    struct parse_node *, struct parse_node *);
struct parse_node *parse_todoy(struct parse_state *,
    struct parse_node *, struct parse_node *, struct parse_node *);
struct parse_node *parse_todoy_list(struct parse_state *,
    struct parse_node *, struct parse_node *);
struct parse_node *parse_rule_allday(struct parse_state *,
    struct parse_node *);
struct parse_node *parse_rule_tod(struct parse_state *,
    struct parse_node *, struct parse_node *, struct parse_node *);
struct parse_node *parse_rule_tow(struct parse_state *,
    struct parse_node *, struct parse_node *, struct parse_node *);
struct parse_node *parse_rule_toy(struct parse_state *,
    struct parse_node *, struct parse_node *, struct parse_node *);
struct parse_node *parse_rule_towoy(struct parse_state *,
    struct parse_node *, struct parse_node *, struct parse_node *);
struct parse_node *parse_rule_lin(struct parse_state *,
    struct parse_node *, struct parse_node *);

#endif /* _PARSER_H */
