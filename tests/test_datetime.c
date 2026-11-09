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

#include <sys/types.h>
#include <signal.h>

#include "test_common.h"

#include "datetime.h"

static struct dt_monthday jan12 = { .day = 12, .month = 0 };
static struct dt_monthday jan13 = { .day = 13, .month = 0 };
static struct dt_monthday feb12 = { .day = 12, .month = 1 };
static struct dt_monthday feb13 = { .day = 13, .month = 1 };

static void
test_dt_month(void)
{
	g_assert_cmpint(MONTH_JAN, ==, 0);
	g_assert_cmpint(MONTH_FEB, ==, 1);
	g_assert_cmpint(MONTH_MAR, ==, 2);
	g_assert_cmpint(MONTH_APR, ==, 3);
	g_assert_cmpint(MONTH_MAY, ==, 4);
	g_assert_cmpint(MONTH_JUN, ==, 5);
	g_assert_cmpint(MONTH_JUL, ==, 6);
	g_assert_cmpint(MONTH_AUG, ==, 7);
	g_assert_cmpint(MONTH_SEP, ==, 8);
	g_assert_cmpint(MONTH_OCT, ==, 9);
	g_assert_cmpint(MONTH_NOV, ==, 10);
	g_assert_cmpint(MONTH_DEC, ==, 11);
}

static void
test_dt_month_valid(void)
{
	int i;

	g_assert_false(DT_MONTH_VALID(-1));
	g_assert_false(DT_MONTH_VALID(12));
	for (i = 0; i < 12; i++)
		g_assert_true(DT_MONTH_VALID(i));
}

static void
test_dt_wday(void)
{
	g_assert_cmpint(WDAY_SUN, ==, 0);
	g_assert_cmpint(WDAY_MON, ==, 1);
	g_assert_cmpint(WDAY_TUE, ==, 2);
	g_assert_cmpint(WDAY_WED, ==, 3);
	g_assert_cmpint(WDAY_THU, ==, 4);
	g_assert_cmpint(WDAY_FRI, ==, 5);
	g_assert_cmpint(WDAY_SAT, ==, 6);
}

static void
test_dt_wday_valid(void)
{
	int i;

	g_assert_false(DT_WDAY_VALID(-1));
	g_assert_false(DT_WDAY_VALID(7));
	for (i = 0; i < 7; i++)
		g_assert_true(DT_WDAY_VALID(i));
}

static struct test_dt_monthday_valid {
	const char *test_name;
	enum dt_month month;
	dt_mday_t maxday;
} tests_dt_monthday_valid[] = {
	{ "jan", MONTH_JAN, 31 }, { "feb", MONTH_FEB, 28 },
	{ "mar", MONTH_MAR, 31 }, { "apr", MONTH_APR, 30 },
	{ "may", MONTH_MAY, 31 }, { "jun", MONTH_JUN, 30 },
	{ "jul", MONTH_JUL, 31 }, { "aug", MONTH_AUG, 31 },
	{ "sep", MONTH_SEP, 30 }, { "oct", MONTH_OCT, 31 },
	{ "nov", MONTH_NOV, 30 }, { "dec", MONTH_DEC, 31 },
};

static void
test_dt_monthday_valid(const struct test_dt_monthday_valid *t)
{
	/* odd-indexed entries are leap years */
	int years[] = { 1995, 2000, 2019, 2024, 2100 };
	struct dt_monthday md;
	size_t i;

	g_assert_cmpint(dt_month_days(t->month), ==, t->maxday);
	md.day = 0;
	md.month = t->month;
	g_assert_false(DT_MONTHDAY_VALID(&md));
	for (md.day = 1; md.day <= t->maxday; md.day++) {
		g_assert_true(DT_MONTHDAY_VALID(&md));
	}
	g_assert_false(DT_MONTHDAY_VALID(&md));

	for (i = 0; i < NITEMS(years); i++) {
		int year = years[i];
		dt_mday_t maxday = t->maxday;
		if (t->month == MONTH_FEB)
			maxday += i % 2;
		g_assert_cmpint(dt_month_days_for_year(t->month, year),
		    ==, maxday);
		md.day = 0;
		g_assert_false(DT_MONTHDAY_VALID_FOR_YEAR(&md, year));
		for (md.day = 1; md.day <= maxday; md.day++)
			g_assert_true(DT_MONTHDAY_VALID_FOR_YEAR(&md, year));
		g_assert_false(DT_MONTHDAY_VALID_FOR_YEAR(&md, year));
	}
}

static void
test_dt_monthday_cmp(void)
{
	struct dt_monthday j13 = { .day = 13, .month = 0 };
	struct dt_monthday f13 = { .day = 13, .month = 1 };

	g_assert_cmpint(dt_monthday_cmp(&jan12, &jan13), <, 0);
	g_assert_cmpint(dt_monthday_cmp(&jan13, &jan12), >, 0);
	g_assert_cmpint(dt_monthday_cmp(&jan13, &j13), ==, 0);
	g_assert_cmpint(dt_monthday_cmp(&j13, &jan13), ==, 0);

	g_assert_cmpint(dt_monthday_cmp(&jan13, &feb12), <, 0);
	g_assert_cmpint(dt_monthday_cmp(&feb12, &jan13), >, 0);
	g_assert_cmpint(dt_monthday_cmp(&feb13, &f13), ==, 0);
	g_assert_cmpint(dt_monthday_cmp(&f13, &feb13), ==, 0);
	g_assert_cmpint(dt_monthday_cmp(&jan13, &feb13), <, 0);
	g_assert_cmpint(dt_monthday_cmp(&feb13, &jan13), >, 0);
}

static void
test_dt_monthday_cmp_tm(void)
{
	struct tm j12, j13, f12, f13;

	memset(&j12, 0xff, sizeof(j12));
	memset(&j13, 0xff, sizeof(j13));
	memset(&f12, 0xff, sizeof(f12));
	memset(&f13, 0xff, sizeof(f13));

	strptime("12 1", "%d %m", &j12);
	strptime("13 1", "%d %m", &j13);
	strptime("12 2", "%d %m", &f12);
	strptime("13 2", "%d %m", &f13);

	g_assert_cmpint(dt_monthday_cmp_tm(&jan12, &j13), <, 0);
	g_assert_cmpint(dt_monthday_cmp_tm(&jan13, &j12), >, 0);
	g_assert_cmpint(dt_monthday_cmp_tm(&jan13, &j13), ==, 0);

	g_assert_cmpint(dt_monthday_cmp_tm(&jan13, &f12), <, 0);
	g_assert_cmpint(dt_monthday_cmp_tm(&feb12, &j13), >, 0);
	g_assert_cmpint(dt_monthday_cmp_tm(&feb13, &f13), ==, 0);
	g_assert_cmpint(dt_monthday_cmp_tm(&jan13, &f13), <, 0);
	g_assert_cmpint(dt_monthday_cmp_tm(&feb13, &j13), >, 0);
}

static void
test_dt_monthday_time(void)
{
	g_assert_cmpint(dt_monthday_time(&jan12, 2000), ==, (time_t)947635200);
	g_assert_cmpint(dt_monthday_time(&jan13, 2000), ==, (time_t)947721600);
	g_assert_cmpint(dt_monthday_time(&feb12, 2000), ==, (time_t)950313600);
	g_assert_cmpint(dt_monthday_time(&feb13, 2000), ==, (time_t)950400000);
}

static void
test_dt_localtime(void)
{
	struct tm tm;
	int ret;

	memset(&tm, 0xff, sizeof(tm));
	ret = dt_localtime((time_t)1790123754, &tm);
	g_assert_cmpint(ret, ==, 0);
	g_assert_cmpint(tm.tm_sec,  ==, 54);
	g_assert_cmpint(tm.tm_min,  ==, 35);
	g_assert_cmpint(tm.tm_hour, ==, 0);
	g_assert_cmpint(tm.tm_mday, ==, 23);
	g_assert_cmpint(tm.tm_mon,  ==, 8);
	g_assert_cmpint(tm.tm_year, ==, 126);
	g_assert_cmpint(tm.tm_wday, ==, 3);
	g_assert_cmpint(tm.tm_yday, ==, 265);
}


static void
test_dt_day_range(void)
{
	struct tm tm;
	time_t start, end;
	int ret;

	ret = dt_localtime((time_t)1791642251, &tm);
	g_assert_cmpint(ret, ==, 0);
	start = end = 12345;
	ret = dt_day_range(&tm, &start, &end);
	g_assert_cmpint(ret, ==, 0);
	g_assert_cmpint(start, ==, (time_t)1791590400);
	g_assert_cmpint(end, ==, (time_t)1791676800);
}

static void
test_dt_day_range_dst(void)
{
	struct tm tm;
	time_t start, end;
	char **env;
	int ret;

	if (g_test_subprocess()) {
		ret = dt_localtime((time_t)1772981119, &tm);
		g_assert_cmpint(ret, ==, 0);
		start = end = 12345;
		ret = dt_day_range(&tm, &start, &end);
		g_assert_cmpint(ret, ==, 0);
		g_assert_cmpint(start, ==, (time_t)1772953200);
		g_assert_cmpint(end, ==, (time_t)1773036000);
		return;
	}

	env = g_get_environ();
	env = g_environ_setenv(env, "TZ", "America/Denver", TRUE);
	env = g_environ_setenv(env, "TEST_KEEP_TZ", "1", TRUE);
	g_test_trap_subprocess_with_envp(NULL, (const char **)env, 0,
	    G_TEST_SUBPROCESS_DEFAULT);
	g_test_trap_assert_passed();
	g_strfreev(env);
}

static volatile time_t got_sigalrm;

static void
handle_sigalrm(int sig)
{
	if (sig == SIGALRM)
		got_sigalrm = time(NULL);
}

static void
test_dt_alarm(void)
{
	int secs[] = { 5, 10, 15, 20, 30, 45, 60, 90, 120, };
	char buf1[32], buf2[32];
	sigset_t sigs;
	size_t i;

	signal(SIGALRM, handle_sigalrm);
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGALRM);
	sigprocmask(SIG_BLOCK, &sigs, NULL);

	for (i = 0; i < NITEMS(secs); i++)  {
		time_t now1, now2, alarm;
		int delta;
		now1 = time(NULL);
		alarm = now1 + secs[i];
		if (dt_alarm_at(alarm) == -1) {
			g_test_failed();
			return;
		}
		log_note("%s (%lld) - want alarm in %d secs at %s (%lld)",
		    dt_ctime(buf1, sizeof(buf1), now1), (long long)now1,
		    secs[i], dt_ctime(buf2, sizeof(buf2), alarm),
		    (long long)alarm);

		got_sigalrm = 0;
		while (got_sigalrm == 0) {
			sigemptyset(&sigs);
			sigsuspend(&sigs);
		}

		now2 = time(NULL);
		delta = got_sigalrm - now1;
		log_note("%s (%lld) - got alarm in %d secs at %s (%lld)",
		    dt_ctime(buf1, sizeof(buf1), now2), (long long)now2,
		    delta, dt_ctime(buf2, sizeof(buf2), got_sigalrm),
		    (long long)got_sigalrm);
	}
}

static gboolean
is_interactive(int argc, char **argv)
{
	int i;

	for (i = 0; i < argc; i++)
		if (strcmp(argv[i], "--interactive") == 0)
			return TRUE;
	return FALSE;
}

int
main(int argc, char *argv[])
{
	gboolean interactive = (is_interactive(argc, argv) ?
	    (putenv("TEST_KEEP_TZ=1"), TRUE): FALSE);
	TEST_INIT_COMMON(&argc, &argv, "/datetime");

	if (interactive)
		TEST_ADD(dt_alarm);
	else {
		TEST_ADD(dt_month);
		TEST_ADD(dt_month_valid);
		TEST_ADD(dt_wday);
		TEST_ADD(dt_wday_valid);
		TEST_ADD_ARRAY(dt_monthday_valid);
		TEST_ADD(dt_monthday_cmp);
		TEST_ADD(dt_monthday_cmp_tm);
		TEST_ADD(dt_monthday_time);
		TEST_ADD(dt_localtime);
		TEST_ADD(dt_day_range);
		TEST_ADD(dt_day_range_dst);
	}

	return g_test_run();
}
