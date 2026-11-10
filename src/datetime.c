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
#include <sys/time.h>
#include <signal.h>
#include <time.h>

#include "datetime.h"
#include "log.h"
#include "scheduler.h"

dt_mday_t
dt_month_days(enum dt_month month)
{
	g_assert(DT_MONTH_VALID(month));
	switch (month) {
	case MONTH_JAN:
	case MONTH_MAR:
	case MONTH_MAY:
	case MONTH_JUL:
	case MONTH_AUG:
	case MONTH_OCT:
	case MONTH_DEC:
		return 31;
	case MONTH_APR:
	case MONTH_JUN:
	case MONTH_SEP:
	case MONTH_NOV:
		return 30;
	case MONTH_FEB:
		return 28;
	}
	/* silence gcc warning about the impossible */
	return 0;
}

dt_mday_t
dt_month_days_for_year(enum dt_month month, int year)
{
	struct tm tm;
	time_t ret;

	if (month != MONTH_FEB)
		return dt_month_days(month);

	memset(&tm, 0, sizeof(tm));
	tm.tm_mday = 29;
	tm.tm_mon = MONTH_FEB;
	tm.tm_year = year - 1900;
	tm.tm_isdst = -1;
	if ((ret = mktime(&tm)) == -1)
		log_error("mktime() failed");
	return (tm.tm_mday == 29 ? 29 : 28);
}

int
dt_monthday_cmp(const struct dt_monthday *a, const struct dt_monthday *b)
{
	if (a->month != b->month)
		return (a->month > b->month ? 1 : -1);
	if (a->day != b->day)
		return (a->day > b->day ? 1 : -1);
	return 0;
}

int
dt_monthday_cmp_tm(const struct dt_monthday *a, const struct tm *tm)
{
	if (a->month != DT_TM_MONTH(tm))
		return (a->month > DT_TM_MONTH(tm) ? 1 : -1);
	if (a->day != DT_TM_MDAY(tm))
		return (a->day > DT_TM_MDAY(tm) ? 1 : -1);
	return 0;
}

int
dt_localtime(time_t now, struct tm *tm)
{
	errno = 0;
	if (localtime_r(&now, tm) != NULL)
		return 0;
	if (errno == 0)
		log_error("localtime() failed");
	else
		log_error_errno("localtime() failed");
	return -1;
}

time_t
dt_monthday_time(const struct dt_monthday *mday, int year)
{
	struct tm tm;
	time_t ret;

	memset(&tm, 0, sizeof(tm));
	tm.tm_mday = mday->day;
	tm.tm_mon = mday->month;
	tm.tm_year = year - 1900;
	tm.tm_isdst = -1;
	if ((ret = mktime(&tm)) == -1)
		log_error("mktime() failed");

	return ret;
}

int
dt_day_range(const struct tm *now, time_t *startp, time_t *endp)
{
	time_t start, end;
	struct tm tm;

	memset(&tm, 0, sizeof(tm));
	tm.tm_mday = now->tm_mday;
	tm.tm_mon = now->tm_mon;
	tm.tm_year = now->tm_year;
	tm.tm_isdst = -1;
	start = mktime(&tm);
	tm.tm_mday++;
	tm.tm_isdst = -1;
	end = mktime(&tm);
	if (start == -1 || end == -1)
		return -1;
	*startp = start;
	*endp = end;
	return 0;
}

char *
dt_ctime(char *buf, size_t len, time_t when)
{
	struct tm tm;

	if (dt_localtime(when, &tm) == -1)
		return NULL;
	strftime(buf, len, "%a %b %e %H:%M:%S %Y %Z", &tm);
	return buf;
}

int
dt_alarm_at(time_t when)
{
	struct itimerval timer;
	time_t now;

	now = time(NULL);
	if (when <= now) {
		raise(SIGALRM);
		return 0;
	}

	memset(&timer, 0, sizeof(timer));
	timer.it_value.tv_sec = when - now;
	errno = 0;
	if (setitimer(ITIMER_REAL, &timer, NULL) == 0)
	    return 0;
	if (errno == 0)
		log_error("setitimer() failed");
	else
		log_error_errno("setitimer() failed");
	return -1;
}
