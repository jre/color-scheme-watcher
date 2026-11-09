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
#include "grammar.h"
#include "parser.h"
#include "scheduler.h"
#include "util.h"

static struct parse_node *node_new(enum parse_node_type);
static void node_free(struct parse_node *);

const struct sched_rule * const *
parse_config(FILE *stream, const char *filename)
{
	struct parse_state state;
	struct parse_node *node;
	struct sched_rule **rules;
	pcc_context_t *ctx;
	size_t rulecount;

	memset(&state, 0, sizeof(state));
	state.filename = filename;
	state.stream = stream;
	state.date_fmt = PARSE_DATE_FMT_DM;

	rules = g_new0(struct sched_rule *, 1);
	rulecount = 0;

	ctx = pcc_create(&state);
	for (node = NULL; !state.fail && pcc_parse(ctx, &node); node = NULL) {
		if (node == NULL)
			continue;
		g_assert(node->type == PARSE_NODE_RULE);
		rules[rulecount++] = g_steal_pointer(&node->u.rule);
		rules = g_renew(struct sched_rule *, rules, rulecount + 1);
		rules[rulecount] = NULL;
		node_free(node);
	}
	pcc_destroy(ctx);

	if (!state.fail)
		return (const struct sched_rule **)rules;
	else {
		scheduler_free_rules((const struct sched_rule **)rules);
		return NULL;
	}
}

void
parse_error(struct parse_state *state, ssize_t line, ssize_t col)
{
	if (line == -1 || col == -1)
		log_error("%s: syntax error", state->filename);
	else
		log_error("%s:%zd:%zd: syntax error",
		    state->filename, line + 1, col + 1);
	state->fail = TRUE;
}

#ifdef PARSE_GRAMMAR_DBG
void
parse_dbg(int event, const char *rule, unsigned int level, size_t pos,
    const char *buf, int len)
{
	static const char *action[] = { "eval", "match", "abort" };

	while (level--)
		putc('\t', stderr);
	fprintf(stderr, "%s %s at %zd", action[event], rule, pos);
	if (len > 0) {
		fputs(": ", stderr);
		while (len > 0) {
			switch (*buf) {
			case '"': fputs("\\\"", stderr); break;
			case '\\': fputs("\\\\", stderr); break;
			case '\n': fputs("\\n", stderr); break;
			default: putc(*buf, stderr); break;
			}
			len--;
			buf++;
		}
	}
	putc('\n', stderr);
}
# endif /* PARSE_DBG */

static struct parse_node *
node_new(enum parse_node_type type)
{
	struct parse_node *node;

	node = g_new0(struct parse_node, 1);
	node->type = type;

	return node;
}

static void
node_free(struct parse_node *node)
{
	switch (node->type) {
	case PARSE_NODE_NONE:
	case PARSE_NODE_SCHEME:
	case PARSE_NODE_UINT:
	case PARSE_NODE_TOD:
	case PARSE_NODE_WDAY:
	case PARSE_NODE_MDAY:
	case PARSE_NODE_MONTH:
	case PARSE_NODE_DOY:
	case PARSE_NODE_TODOY:
		break;
	case PARSE_NODE_TODOY_LIST:
		g_assert(node->u.todoy_list.days == NULL);	
		break;
	case PARSE_NODE_RULE:
		g_assert(node->u.rule == NULL);
		break;
	}
	g_free(node);
}

struct parse_node *
parse_date_fmt(struct parse_state *state, enum parse_date_fmt date_fmt)
{
	state->date_fmt = date_fmt;
	return NULL;
}

struct parse_node *
parse_scheme(struct parse_state *state G_GNUC_UNUSED, enum color_scheme scheme)
{
	struct parse_node *node = node_new(PARSE_NODE_SCHEME);
	node->u.scheme = scheme;
	return node;
}

struct parse_node *
parse_uint_str(struct parse_state *state G_GNUC_UNUSED, const char *str)
{
	struct parse_node *node = node_new(PARSE_NODE_UINT);
	STR_TO_INT_ELSE(str, unsigned int, node->u.uint,
		g_assert_not_reached();
	);
	return node;
}

struct parse_node *
parse_tod(struct parse_state *state G_GNUC_UNUSED,
    struct parse_node *hour, struct parse_node *minute, enum parse_ampm ampm)
{
	struct parse_node *node = node_new(PARSE_NODE_TOD);
	guint hod, mod;

	g_assert(hour != NULL && hour->type == PARSE_NODE_UINT);
	switch (ampm) {
	case PARSE_AM:
		hod = (hour->u.uint == 12 ? 0 : hour->u.uint);
		break;
	case PARSE_PM:
		hod = (hour->u.uint == 12 ? 0 : hour->u.uint) + 12;
		break;
	case PARSE_24H:
		hod = hour->u.uint;
		break;
	}
	g_assert(hod >= 0 && hod < 24);
	node_free(hour);

	mod = hod * 60;
	if (minute != NULL) {
		g_assert(minute->type == PARSE_NODE_UINT);
		g_assert(minute->u.uint >= 0 && minute->u.uint < 60);
		mod += minute->u.uint;
		node_free(minute);
	}

	node->u.uint = mod * 60;
	return node;
}

struct parse_node *
parse_mday_str(struct parse_state *state G_GNUC_UNUSED, const char *str)
{
	struct parse_node *node = node_new(PARSE_NODE_MDAY);
	STR_TO_INT_ELSE(str, dt_mday_t, node->u.mday,
		g_assert_not_reached();
	);
	return node;
}

struct parse_node *
parse_wday(struct parse_state *state G_GNUC_UNUSED, enum dt_wday wday)
{
	struct parse_node *node = node_new(PARSE_NODE_WDAY);
	g_assert(DT_WDAY_VALID(wday));
	node->u.wday = wday;
	return node;
}

struct parse_node *
parse_month(struct parse_state *state G_GNUC_UNUSED, enum dt_month month)
{
	struct parse_node *node = node_new(PARSE_NODE_MONTH);
	g_assert(DT_MONTH_VALID(month));
	node->u.month = month;
	return node;
}

struct parse_node *
parse_month_str(struct parse_state *state, const char *str)
{
	enum dt_month month;
	STR_TO_INT_ELSE(str, enum dt_month, month,
		g_assert_not_reached();
	);
	return parse_month(state, month);
}

struct parse_node *
parse_doy(struct parse_state *state G_GNUC_UNUSED,
    struct parse_node *day, struct parse_node *month)
{
	struct parse_node *node;
	g_assert(day != NULL && day->type == PARSE_NODE_MDAY);
	g_assert(month != NULL && month->type == PARSE_NODE_MONTH);

	node = node_new(PARSE_NODE_DOY);
	node->u.doy.day = day->u.mday;
	node->u.doy.month = month->u.month;
	g_assert(DT_MONTHDAY_VALID(&node->u.doy));

	node_free(day);
	node_free(month);
	return node;
}

struct parse_node *
parse_todoy(struct parse_state *state G_GNUC_UNUSED,
    struct parse_node *start_time, struct parse_node *end_time,
    struct parse_node *doy)
{
	struct parse_node *node;
	g_assert(start_time != NULL && start_time->type == PARSE_NODE_TOD);
	g_assert(end_time != NULL && end_time->type == PARSE_NODE_TOD);
	g_assert(doy != NULL && doy->type == PARSE_NODE_DOY);

	node = node_new(PARSE_NODE_TODOY);
	node->u.todoy.time.start = start_time->u.uint;
	node->u.todoy.time.end = end_time->u.uint;
	node->u.todoy.mday = doy->u.doy;

	node_free(start_time);
	node_free(end_time);
	node_free(doy);
	return node;
}

struct parse_node *
parse_todoy_list(struct parse_state *state,
    struct parse_node *node, struct parse_node *todoy)
{
	struct dt_time_range_monthday_list *list;
	g_assert(todoy != NULL && todoy->type == PARSE_NODE_TODOY);

	if (node == NULL) {
		node = node_new(PARSE_NODE_TODOY_LIST);
		node->u.todoy_list.count = 1;
		node->u.todoy_list.days =
		    g_new0(struct dt_time_range_monthday, 1);
		node->u.todoy_list.days[0] = todoy->u.todoy;
	} else {
		list = &(node->u.todoy_list);
		if (list->count > 0 &&
		    dt_monthday_cmp(&list->days[list->count-1].mday,
			&todoy->u.todoy.mday) >= 0)
			parse_error(state, state->last_line, state->last_col);
		list->count++;
		list->days = g_renew(struct dt_time_range_monthday,
		    list->days, list->count);
		list->days[list->count-1] = todoy->u.todoy;
	}

	node_free(todoy);
	return node;
}

static struct parse_node *
node_rule_new(struct parse_state *state,
    enum sched_rule_type type, enum color_scheme scheme)
{
	struct parse_node *node = node_new(PARSE_NODE_RULE);
	node->u.rule = g_new0(struct sched_rule, 1);
	node->u.rule->filename = g_strdup(state->filename);
	node->u.rule->line = state->last_line;
	node->u.rule->col = state->last_col;
	node->u.rule->type = type;
	node->u.rule->scheme = scheme;
	return node;
}

struct parse_node *
parse_rule_allday(struct parse_state *state, struct parse_node *scheme)
{
	struct parse_node *node;
	g_assert(scheme != NULL && scheme->type == PARSE_NODE_SCHEME);

	node = node_rule_new(state, SCHED_RULE_ALWAYS, scheme->u.scheme);

	node_free(scheme);
	return node;
}

struct parse_node *
parse_rule_tod(struct parse_state *state, struct parse_node *scheme,
    struct parse_node *start, struct parse_node *end)
{
	struct parse_node *node;
	g_assert(scheme != NULL && scheme->type == PARSE_NODE_SCHEME);
	g_assert(start != NULL && start->type == PARSE_NODE_TOD);
	g_assert(end != NULL && end->type == PARSE_NODE_TOD);

	node = node_rule_new(state, SCHED_RULE_TIME_OF_DAY, scheme->u.scheme);
	node->u.rule->u.time.start = start->u.uint;
	node->u.rule->u.time.end = end->u.uint;

	node_free(scheme);
	node_free(start);
	node_free(end);
	return node;
}

struct parse_node *
parse_rule_tow(struct parse_state *state G_GNUC_UNUSED, struct parse_node *tod,
    struct parse_node *start, struct parse_node *end)
{
	struct dt_time_range time;
	g_assert(tod != NULL && tod->type == PARSE_NODE_RULE &&
	    tod->u.rule->type == SCHED_RULE_TIME_OF_DAY);
	g_assert(start != NULL && start->type == PARSE_NODE_WDAY);
	g_assert(end != NULL && end->type == PARSE_NODE_WDAY);

	time = tod->u.rule->u.time;
	tod->u.rule->type = SCHED_RULE_TIME_OF_WEEK;
	tod->u.rule->u.week.time = time;
	tod->u.rule->u.week.wday.start = start->u.wday;
	tod->u.rule->u.week.wday.end = end->u.wday;

	node_free(start);
	node_free(end);
	return tod;
}

struct parse_node *
parse_rule_toy(struct parse_state *state G_GNUC_UNUSED, struct parse_node *tod,
    struct parse_node *start, struct parse_node *end)
{
	struct dt_time_range time;
	g_assert(tod != NULL && tod->type == PARSE_NODE_RULE &&
	    tod->u.rule->type == SCHED_RULE_TIME_OF_DAY);
	g_assert(start != NULL && start->type == PARSE_NODE_DOY);
	g_assert(end != NULL && end->type == PARSE_NODE_DOY);

	time = tod->u.rule->u.time;
	tod->u.rule->type = SCHED_RULE_TIME_OF_YEAR;
	tod->u.rule->u.year.time = time;
	tod->u.rule->u.year.mday.start = start->u.doy;
	tod->u.rule->u.year.mday.end = end->u.doy;

	node_free(start);
	node_free(end);
	return tod;
}

struct parse_node *
parse_rule_towoy(struct parse_state *state G_GNUC_UNUSED,
    struct parse_node *rule, struct parse_node *start, struct parse_node *end)
{
	struct dt_time_range time;
	struct dt_weekday_range wday;
	struct dt_monthday_range mday;
	g_assert(rule != NULL && rule->type == PARSE_NODE_RULE);
	g_assert(start != NULL && end != NULL);

	switch (rule->u.rule->type) {
	case SCHED_RULE_TIME_OF_WEEK:
		g_assert(start->type == PARSE_NODE_DOY);
		g_assert(end->type == PARSE_NODE_DOY);
		time = rule->u.rule->u.week.time;
		wday = rule->u.rule->u.week.wday;
		mday.start = start->u.doy;
		mday.end = end->u.doy;
		break;
	case SCHED_RULE_TIME_OF_YEAR:
		g_assert(start->type == PARSE_NODE_WDAY);
		g_assert(end->type == PARSE_NODE_WDAY);
		time = rule->u.rule->u.year.time;
		mday = rule->u.rule->u.year.mday;
		wday.start = start->u.wday;
		wday.end = end->u.wday;
		break;
	default:
		g_assert_not_reached();
		break;
	}
	rule->u.rule->type = SCHED_RULE_TIME_OF_YEAR_WEEK;
	rule->u.rule->u.yearweek.time = time;
	rule->u.rule->u.yearweek.wday = wday;
	rule->u.rule->u.yearweek.mday = mday;

	node_free(start);
	node_free(end);
	return rule;
}

struct parse_node *
parse_rule_lin(struct parse_state *state, struct parse_node *scheme,
    struct parse_node *list)
{
	struct parse_node *node;
	g_assert(scheme != NULL && scheme->type == PARSE_NODE_SCHEME);
	g_assert(list != NULL && list->type == PARSE_NODE_TODOY_LIST);

	node = node_rule_new(state, SCHED_RULE_LINEAR, scheme->u.scheme);
	node->u.rule->u.linear = list->u.todoy_list;
	list->u.todoy_list.days = NULL;
	node_free(scheme);
	node_free(list);

	return node;
}
