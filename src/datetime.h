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

#ifndef _DATETIME_H
#define _DATETIME_H

#include <stdlib.h>
#include <time.h>

/* large enough for "www mmm dd HH:MM:SS yyyy zzz" */
#define DT_CTIME_LEN	(29)

/* compatible with struct tm tm_mon field */
enum dt_month {
	MONTH_JAN, MONTH_FEB, MONTH_MAR, MONTH_APR, MONTH_MAY, MONTH_JUN,
	MONTH_JUL, MONTH_AUG, MONTH_SEP, MONTH_OCT, MONTH_NOV, MONTH_DEC
};
#define DT_MONTH_VALID(v)	((v) >= MONTH_JAN && (v) <= MONTH_DEC)

/* compatible with struct tm tm_wday field */
enum dt_wday {
	WDAY_SUN, WDAY_MON, WDAY_TUE, WDAY_WED, WDAY_THU, WDAY_FRI, WDAY_SAT
};
#define DT_WDAY_VALID(v)	((v) >= WDAY_SUN && (v) <= WDAY_SAT)

/* day of month, starting at 1 */
typedef unsigned int dt_mday_t;

struct dt_monthday {
	dt_mday_t day;
	enum dt_month month;
};
#define DT_MONTHDAY_VALID(v)						\
	((v)->day >= 1 && (v)->day <= dt_month_days((v)->month))
#define DT_MONTHDAY_VALID_FOR_YEAR(v, y)					\
	((v)->day >= 1 && (v)->day <= dt_month_days_for_year((v)->month, (y)))

/* time of day in seconds */
typedef unsigned int dt_tod_t;

struct dt_time_range {
	dt_tod_t start, end;
};

struct dt_weekday_range {
	enum dt_wday start, end;
};

struct dt_monthday_range {
	struct dt_monthday start, end;
};

struct dt_time_range_monthday {
	struct dt_time_range time;
	struct dt_monthday mday;
};

struct dt_time_range_monthday_list {
	struct dt_time_range_monthday *days;
	size_t count;
};

#define DT_TM_MDAY(t)	((dt_mday_t)(t)->tm_mday)
#define DT_TM_MONTH(t)	((enum dt_month)(t)->tm_mon)
#define DT_TM_YEAR(t)	((t)->tm_year + 1900)
#define DT_TM_WDAY(t)	((enum dt_wday)(t)->tm_wday)

dt_mday_t dt_month_days(enum dt_month);
dt_mday_t dt_month_days_for_year(enum dt_month, int);
int dt_monthday_cmp(const struct dt_monthday *, const struct dt_monthday *);
int dt_monthday_cmp_tm(const struct dt_monthday *, const struct tm *);
time_t dt_monthday_time(const struct dt_monthday *, int);
int dt_localtime(time_t, struct tm *);
int dt_day_range(const struct tm *, time_t *, time_t *);
int dt_alarm_at(time_t);
char *dt_ctime(char *, size_t, time_t);

#endif /* _DATETIME_H */
