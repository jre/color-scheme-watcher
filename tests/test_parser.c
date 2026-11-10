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

#include "test_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "parser.h"
#include "util.h"

G_GNUC_PRINTF(3, 4)
static void
appendf(char *buf, size_t size, const char *format, ...)
{
	va_list ap;
	char *eos;

	if ((eos = memchr(buf, '\0', size)) == NULL)
		return;
	size -= eos - buf;
	buf = eos;

	va_start(ap, format);
	g_vsnprintf(buf, size, format, ap);
	va_end(ap);
}

static void
append_time_range(const struct dt_time_range *time, char *buf, size_t size)
{
	appendf(buf, size, "%u:%02u:%02u-%u:%02u:%02u",
	    time->start / 60 / 60, time->start / 60 % 60, time->start % 60,
	    time->end / 60 / 60, time->end / 60 % 60, time->end % 60);
}

static void
append_weekday_range(const struct dt_weekday_range *wday,
    char *buf, size_t size)
{
	static const char *names[] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};

	if (wday->start < NITEMS(names))
		g_strlcat(buf, names[wday->start], size);
	else
		appendf(buf, size, "<%d>", wday->start);
	g_strlcat(buf, "-", size);
	if (wday->end < 7)
		g_strlcat(buf, names[wday->end], size);
	else
		appendf(buf, size, "<%d>", wday->end);
}

static void
append_monthday(const struct dt_monthday *mday,
    char *buf, size_t size)
{
	static const char *names[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	if (mday->month < NITEMS(names))
		g_strlcat(buf, names[mday->month], size);
	else
		appendf(buf, size, "<%d>", mday->month);
	appendf(buf, size, "%u", mday->day);
}

static void
append_monthday_range(const struct dt_monthday_range *mday,
    char *buf, size_t size)
{
	append_monthday(&mday->start, buf, size);
	g_strlcat(buf, "-", size);
	append_monthday(&mday->end, buf, size);
}

static void
append_time_range_monthday_list(const struct dt_time_range_monthday_list *list,
    char *buf, size_t size)
{
	size_t i;

	for (i = 0; i < list->count; i++) {
		append_time_range(&list->days[i].time, buf, size);
		g_strlcat(buf, " ", size);
		append_monthday(&list->days[i].mday, buf, size);
		if (i + 1 < list->count)
			g_strlcat(buf, ", ", size);
	}
}

static void
rule_str(const struct sched_rule *rule, char *buf, size_t size)
{
	if (rule == NULL)
		return;

	g_strlcat(buf, color_scheme_name(rule->scheme), size);
	switch (rule->type) {
	case SCHED_RULE_ALWAYS:
		g_strlcat(buf, " always", size);
		break;
	case SCHED_RULE_TIME_OF_DAY:
		g_strlcat(buf, " tod ", size);
		append_time_range(&rule->u.time, buf, size);
		break;
	case SCHED_RULE_TIME_OF_WEEK:
		g_strlcat(buf, " tow ", size);
		append_time_range(&rule->u.week.time, buf, size);
		g_strlcat(buf, " ", size);
		append_weekday_range(&rule->u.week.wday, buf, size);
		break;
	case SCHED_RULE_TIME_OF_YEAR:
		g_strlcat(buf, " toy ", size);
		append_time_range(&rule->u.year.time, buf, size);
		g_strlcat(buf, " ", size);
		append_monthday_range(&rule->u.year.mday, buf, size);
		break;
	case SCHED_RULE_TIME_OF_YEAR_WEEK:
		g_strlcat(buf, " toyw ", size);
		append_time_range(&rule->u.yearweek.time, buf, size);
		g_strlcat(buf, " ", size);
		append_weekday_range(&rule->u.yearweek.wday, buf, size);
		g_strlcat(buf, " ", size);
		append_monthday_range(&rule->u.yearweek.mday, buf, size);
		break;
	case SCHED_RULE_LINEAR:
		g_strlcat(buf, " lin ", size);
		append_time_range_monthday_list(&rule->u.linear, buf, size);
		break;
	default:
		g_assert_false("unknown rule id");
		break;
	}
}

static struct test_parse_config {
	const char *test_name; const char *input; const char *output;
} tests_parse_config[] = {
	{ "empty 1", "", "" },
	{ "empty 2", "  \n\t ", "" },
	{ "empty 3", "#foo", "" },
	{ "empty 4", "\n  #bar\n\n\t", "" },
	{ "empty 5", "#baz\r\n  \f\v", "" },
	{ "const dark 1", "Use Dark Mode", "dark always\n" },
	{ "const dark 2", "USE nighT Mode", "dark always\n" },
	{ "const light 1", " use\tLiGhT #foo\n moDe  ", "light always\n" },
	{ "const light 2", "\n USE\rDAY\vMODE  ", "light always\n" },
	{ "const def 1", "usE DEFault Mode", "default always\n" },
	{ "const def 2", "use DEF mode\n", "default always\n" },
	/* XXX */
};

static void
test_parse_config(const struct test_parse_config *t)
{
	const struct sched_rule * const *rules;
	char buf[1024];
	size_t ilen, i;
	FILE *fh;

	ilen = strlen(t->input);
	if (ilen == 0)
		fh = fmemopen(NULL, 1, "w+");
	else
		fh = fmemopen((void *)t->input, strlen(t->input), "r");
	if (fh == NULL) {
		perror("fmemopen");
		g_test_fail();
		return;
	}
	rules = parse_config(fh, t->test_name);
	fclose(fh);

	g_assert_nonnull(rules);
	buf[0] = '\0';
	for (i = 0; rules[i] != NULL; i++) {
		rule_str(rules[i], buf, sizeof(buf));
		strlcat(buf, "\n", sizeof(buf));
	}
	g_assert_cmpstr(t->output, ==, buf);

	scheduler_free_rules(rules);
}

int
main(int argc, char *argv[])
{
	TEST_INIT_COMMON(&argc, &argv, "/parser");

	TEST_ADD_ARRAY(parse_config);

	return g_test_run();
}
