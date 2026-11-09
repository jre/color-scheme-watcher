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

#include "log.h"
#include "scheduler.h"

struct sched_rule_match {
	enum color_scheme scheme;
	struct dt_time_range time;
};

struct sched_event {
	enum color_scheme scheme;
	time_t time;
	struct sched_event *next;
};

struct scheduler {
	const struct sched_rule * const *rules;
	const struct sched_event *plan;
};

#define IN_RANGE_INCL(start, end, thing) \
	((start) <= (end) ? \
	 ((start) <= (thing) && (thing) <= (end)) : \
	 ((thing) <= (start) || (end) <= (thing)))

#define MONTHDAY_IN_RANGE_INCL(r, t) \
	(dt_monthday_cmp(&(r)->start, &(r)->end) <= 0 ? \
	 (dt_monthday_cmp_tm(&(r)->start, (t)) <= 0 && \
	  dt_monthday_cmp_tm(&(r)->end, (t)) >= 0) : \
	 (dt_monthday_cmp_tm(&(r)->start, (t)) <= 0 ||		\
	  dt_monthday_cmp_tm(&(r)->end, (t)) >= 0))

static const struct dt_time_range time_range_always = {
	.start = 0, .end = UINT_MAX
};

static gboolean
rule_match(const struct sched_rule *rule, const struct tm *tm)
{
	switch (rule->type) {
	case SCHED_RULE_ALWAYS:
	case SCHED_RULE_TIME_OF_DAY:
		return TRUE;
	case SCHED_RULE_TIME_OF_WEEK:
		return IN_RANGE_INCL(rule->u.week.wday.start,
		    rule->u.week.wday.end, DT_TM_WDAY(tm));
	case SCHED_RULE_TIME_OF_YEAR:
		return MONTHDAY_IN_RANGE_INCL(&rule->u.year.mday, tm);
	case SCHED_RULE_TIME_OF_YEAR_WEEK:
		return IN_RANGE_INCL(rule->u.yearweek.wday.start,
		    rule->u.yearweek.wday.end, DT_TM_WDAY(tm)) &&
		    MONTHDAY_IN_RANGE_INCL(&rule->u.yearweek.mday, tm);
	case SCHED_RULE_LINEAR:
		return TRUE;
	}
}

static dt_tod_t
linear_solve_y(time_t x1, dt_tod_t y1, time_t x2, dt_tod_t y2, time_t rx)
{
	long double slope;

	slope = ((long double)(y2 - y1)) / ((long double)(x2 - x1));
	/*
	  This is just the slope equation, solved for ry:
	    (ry - y1) / (rx - x1) == slope
	    ry - y1 == slope * (rx - x1)
	    ry == (slope * (rx - x1)) + y1
	*/
	return ((dt_tod_t)(slope * ((long double)(rx - x1)))) + y1;
}

static struct dt_time_range
rule_time_range_linear(const struct dt_time_range_monthday *days,
    size_t count, const struct tm *now)
{
	const struct dt_time_range *prev_range, *cur_range;
	time_t prev_day, cur_day, today;
	struct dt_time_range ret;
	struct dt_monthday md;
	size_t i;
	int year;

	g_assert(count > 0);
	year = DT_TM_YEAR(now);
	md.day = DT_TM_MDAY(now);
	md.month = DT_TM_MONTH(now);
	today = dt_monthday_time(&md, year);
	prev_day = dt_monthday_time(&days[count-1].mday, year - 1);
	prev_range = &days[count-1].time;
	if (today == -1 || prev_day == -1)
		return time_range_always;

	for (i = 0; i <= count; i++) {
		if (i == count) {
			cur_day = dt_monthday_time(&days[0].mday, year + 1);
			cur_range = &days[0].time;
		} else {
			cur_day = dt_monthday_time(&days[i].mday, year);
			cur_range = &days[i].time;
		}
		if (cur_day == -1)
			return time_range_always;
		if (IN_RANGE_INCL(prev_day, cur_day, today)) {
			ret.start = linear_solve_y(prev_day, prev_range->start,
			    cur_day, cur_range->start, today);
			ret.end = linear_solve_y(prev_day, prev_range->end,
			    cur_day, cur_range->end, today);
			return ret;
		}
		prev_day = cur_day;
		prev_range = cur_range;
	}

	g_assert_not_reached();
}

static struct dt_time_range
rule_time_range(const struct sched_rule *rule, const struct tm *now)
{
	switch (rule->type) {
	case SCHED_RULE_ALWAYS:
		return time_range_always;
	case SCHED_RULE_TIME_OF_DAY:
		return rule->u.time;
	case SCHED_RULE_TIME_OF_WEEK:
		return rule->u.week.time;
	case SCHED_RULE_TIME_OF_YEAR:
		return rule->u.year.time;
	case SCHED_RULE_TIME_OF_YEAR_WEEK:
		return rule->u.yearweek.time;
	case SCHED_RULE_LINEAR:
		return rule_time_range_linear(rule->u.linear.days,
		    rule->u.linear.count, now);
	}
}

static void
find_rule_matches(const struct sched_rule * const *rules,
    struct sched_rule_match matches[N_COLOR_SCHEMES], const struct tm *now)
{
	unsigned int found;
	size_t i, match_idx;

	for (i = 0; i < N_COLOR_SCHEMES; i++)
		matches[i].scheme = SCHEME_UNKNOWN;
	match_idx = 0;

	found = 0;
	for (i = 0; rules != NULL && rules[i] != NULL; i++) {
		if ((COLOR_SCHEME_BIT(rules[i]->scheme) & found) ||
		    !rule_match(rules[i], now))
			continue;
		found |= COLOR_SCHEME_BIT(rules[i]->scheme);
		matches[match_idx].scheme = rules[i]->scheme;
		matches[match_idx].time = rule_time_range(rules[i], now);
		match_idx++;
	}
}

static struct sched_event *
insert_event(struct sched_event *ev, enum color_scheme scheme, time_t time)
{
	struct sched_event *new;

	new = g_new0(struct sched_event, 1);
	new->scheme = scheme;
	new->time = time;
	if (ev != NULL) {
		new->next = ev->next;
		ev->next = new;
	}

	return new;
}

static struct sched_event *
merge_event(struct sched_event *plan, time_t day_start, time_t day_end,
    enum color_scheme scheme, dt_tod_t rel_start, dt_tod_t rel_end)
{
	struct sched_event *ev;
	time_t start, end;

	start = day_start + rel_start;
	g_assert(day_end > day_start);
	end = day_start + MIN((dt_tod_t)(day_end - day_start), rel_end);

	for (ev = plan; ev != NULL; ev = ev->next) {
		time_t ev_end = (ev->next == NULL ? day_end : ev->next->time);
		if (ev_end <= start)
			continue;
		g_assert(ev->time <= start);

		if (ev->scheme == SCHEME_UNKNOWN) {
			if (ev->time == start)
				ev->scheme = scheme;
			else
				ev = insert_event(ev, scheme, start);
			if (end < ev_end)
				ev = insert_event(ev, SCHEME_UNKNOWN, end);
		}
		start = MIN(ev_end, end);

		if (start == end)
			break;
	}
	g_assert(start == end);

	return plan;
}

static struct sched_event *
finalize_plan(struct sched_event *plan, time_t day_end)
{
	struct sched_event *ev;

	for (ev = plan; ev != NULL; ev = ev->next)
		if (ev->scheme == SCHEME_UNKNOWN)
			ev->scheme = SCHEME_DEFAULT;

	g_assert(plan != NULL);
	for (ev = plan; ev->next != NULL; ev = ev->next) {
		while (ev->next != NULL && ev->scheme == ev->next->scheme) {
			struct sched_event *dead = ev->next;
			ev->next = dead->next;
			g_free(dead);
		}
		g_assert(ev->next == NULL || ev->time < ev->next->time);
	}

	g_assert(ev != NULL && ev->next == NULL);
	g_assert(ev->time < day_end);
	insert_event(ev, SCHEME_UNKNOWN, day_end);

	if (log_debug_enabled()) {
		char buf[DT_CTIME_LEN];
		for (ev = plan; ev != NULL; ev = ev->next)
			log_debug("planning %-7s at %s",
			    (ev->scheme == SCHEME_UNKNOWN ? "replan" :
				color_scheme_name(ev->scheme)),
			    dt_ctime(buf, sizeof(buf), ev->time));
	}

	return plan;
}

static struct sched_event *
plan_day(const struct sched_rule * const *rules, time_t now)
{
	struct sched_rule_match matches[N_COLOR_SCHEMES];
	struct sched_event *plan;
	time_t day_start, day_end;
	struct tm tm;
	size_t i;

	if (log_debug_enabled()) {
		char buf[DT_CTIME_LEN];
		log_debug("replan at %s", dt_ctime(buf, sizeof(buf), now));
	}

	if (dt_localtime(now, &tm) == -1 ||
	    dt_day_range(&tm, &day_start, &day_end) == -1)
		return NULL;
	find_rule_matches(rules, matches, &tm);

	plan = insert_event(NULL, SCHEME_UNKNOWN, day_start);
	for (i = 0; i < N_COLOR_SCHEMES; i++) {
		struct sched_rule_match *m = &(matches[i]);
		if (m->scheme == SCHEME_UNKNOWN)
			continue;
		if (m->time.start <= m->time.end)
			plan = merge_event(plan, day_start, day_end,
			    m->scheme, m->time.start, m->time.end);
		else {
			plan = merge_event(plan, day_start, day_end,
			    m->scheme, 0, m->time.end);
			plan = merge_event(plan, day_start, day_end,
			    m->scheme, m->time.start, UINT_MAX);
		}
	}

	return finalize_plan(plan, day_end);
}

const struct sched_event *
find_current_event(const struct sched_event *ev, time_t now)
{
	while (ev != NULL) {
		if (ev->time <= now &&
		    (ev->next == NULL || ev->next->time > now))
			return ev;
		ev = ev->next;
	}

	return NULL;
}

static void
free_events(const struct sched_event *list)
{
	struct sched_event *next;

	while (list != NULL) {
		next = list->next;
		g_free((void *)list);
		list = next;
	}
}

void
scheduler_free_rules(const struct sched_rule * const *rules)
{
	size_t i;

	if (rules == NULL)
		return;

	for (i = 0; rules[i] != NULL; i++) {
		switch (rules[i]->type) {
		case SCHED_RULE_ALWAYS:
		case SCHED_RULE_TIME_OF_DAY:
		case SCHED_RULE_TIME_OF_WEEK:
		case SCHED_RULE_TIME_OF_YEAR:
		case SCHED_RULE_TIME_OF_YEAR_WEEK:
			break;
		case SCHED_RULE_LINEAR:
			g_free(rules[i]->u.linear.days);
			break;
		}
		g_free((void *)rules[i]);
	}
	g_free((void *)rules);
}

struct scheduler *
scheduler_create(void)
{
	return g_new0(struct scheduler, 1);
}

void
scheduler_set_rules(struct scheduler *sched,
    const struct sched_rule * const *rules)
{
	scheduler_free_rules(sched->rules);
	sched->rules = rules;
	free_events(g_steal_pointer(&sched->plan));
}

enum color_scheme
scheduler_query_at(struct scheduler *sched, time_t now, time_t *next_change)
{
	const struct sched_event *ev;

	if (sched->rules == NULL) {
		log_debug("no scheduler rules; not planning scheme changes");
		*next_change = -1;
		return SCHEME_UNKNOWN;
	}

	if ((ev = find_current_event(sched->plan, now)) == NULL ||
	    ev->scheme == SCHEME_UNKNOWN || ev->next == NULL) {
		free_events(sched->plan);
		sched->plan = plan_day(sched->rules, now);
		ev = find_current_event(sched->plan, now);
	}

	if (ev == NULL || ev->next == NULL) {
		log_warn("failed to plan scheme change, retry in 1 hour");
		*next_change = now + 60 * 60;
		return SCHEME_UNKNOWN;
	}

	g_assert(now < ev->next->time);
	*next_change = ev->next->time;
	return ev->scheme;
}

enum color_scheme
scheduler_query(struct scheduler *sched, time_t *next_change)
{
	return scheduler_query_at(sched, time(NULL), next_change);
}

void
scheduler_free(struct scheduler *sched)
{
	scheduler_free_rules(sched->rules);
	free_events(sched->plan);
	g_free(sched);
}
