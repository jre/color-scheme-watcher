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

#include <stdio.h>

#include "datetime.h"
#include "scheduler.h"
#include "test_common.h"

/* include the actual C file to test static functions */
#include "scheduler.c"

#define TOD_SECS_H(h)				((h)*60*60)
#define TOD_SECS_HM(h, m)			((((h)*60)+(m))*60)

#define RULENAM(nam)				test_rule_##nam
#define RULESET(nam)				test_ruleset_##nam

#define DEFRULE(n, t, s, ...)	static const struct sched_rule		\
	RULENAM(n) = { #n, 0, 0, SCHED_RULE_##t, SCHEME_##s, __VA_ARGS__ }
#define DEFRULE_ALWAYS(n, s)			DEFRULE(n, ALWAYS, s, {0})
#define DEFRULE_TOD_H(n, sc, sh, eh)		DEFRULE(n, TIME_OF_DAY, sc, \
	    .u.time.start=TOD_SECS_H(sh), .u.time.end=TOD_SECS_H(eh))
#define DEFRULE_TOD_HM(n, sc, shm, ehm) 	DEFRULE(n, TIME_OF_DAY, sc, \
	    .u.time.start=TOD_SECS_HM(shm), .u.time.end=TOD_SECS_HM(ehm))

#define RULE(n)					&RULENAM(n)
#define DEFRULESET(n, ...)	static const struct sched_rule *const	\
	RULESET(n)[] = { __VA_ARGS__, NULL }

DEFRULE_ALWAYS(dark, DARK);

DEFRULE_TOD_H(light_9am_5pm, LIGHT, 9, 17);
DEFRULE_TOD_H(dark_8pm_6am, DARK, 20, 6);
DEFRULE_TOD_H(default_6am_8pm, DEFAULT, 6, 20);

DEFRULESET(simple_implicit_default,
    RULE(light_9am_5pm), RULE(dark_8pm_6am));
DEFRULESET(simple_explicit_default,
    RULE(light_9am_5pm), RULE(default_6am_8pm), RULE(dark));

static struct sched_event test_events_simple_oct3[] = {
	{ SCHEME_DARK, 1790985600, 0 },
	{ SCHEME_DEFAULT, 1791007200, 0 },
	{ SCHEME_LIGHT, 1791018000, 0 },
	{ SCHEME_DEFAULT, 1791046800, 0 },
	{ SCHEME_DARK, 1791057600, 0 },
	{ SCHEME_UNKNOWN, 1791072000, 0 },
};

static struct test_plan_day {
	time_t day;
	const struct sched_rule * const *rules;
	struct sched_event *events;
	const char *test_name;
} tests_plan_day[] = {
	{ 1791041791, RULESET(simple_implicit_default),
	  test_events_simple_oct3, "simple-oct-3-implicit" },
	{ 1791041791, RULESET(simple_explicit_default),
	  test_events_simple_oct3, "simple-oct-3-explicit" },
	/*  XXX */
};

static void
test_plan_day(const struct test_plan_day *t)
{
	struct sched_event *events, *ev;
	size_t i;

	events = plan_day(t->rules, t->day);
	for (ev = events, i = 0; ev != NULL; ev = ev->next, i++) {
		g_assert_cmpint(ev->time, ==, t->events[i].time);
		g_assert_cmpint(ev->scheme, ==, t->events[i].scheme);
	}
	free_events(events);
}

int
main(int argc, char *argv[])
{
	TEST_INIT_COMMON(&argc, &argv, "/scheduler");

	TEST_ADD_ARRAY(plan_day);

	return g_test_run();
}
