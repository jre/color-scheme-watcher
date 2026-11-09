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

#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include "color_scheme.h"
#include "datetime.h"

struct scheduler;

struct sched_rule {
	const char *filename;
	ssize_t line, col;
	enum sched_rule_type {
		SCHED_RULE_ALWAYS = 1,
		SCHED_RULE_TIME_OF_DAY,
		SCHED_RULE_TIME_OF_WEEK,
		SCHED_RULE_TIME_OF_YEAR,
		SCHED_RULE_TIME_OF_YEAR_WEEK,
		SCHED_RULE_LINEAR,
	} type;
	enum color_scheme scheme;
	union {
		struct dt_time_range time;
		struct {
			struct dt_time_range time;
			struct dt_weekday_range wday;
		} week;
		struct {
			struct dt_time_range time;
			struct dt_monthday_range mday;
		} year;
		struct {
			struct dt_time_range time;
			struct dt_weekday_range wday;
			struct dt_monthday_range mday;
		} yearweek;
		struct dt_time_range_monthday_list linear;
	} u;
};

struct scheduler *scheduler_create(void);
void scheduler_set_rules(struct scheduler *, const struct sched_rule * const*);
enum color_scheme scheduler_query(struct scheduler *, time_t *);
enum color_scheme scheduler_query_at(struct scheduler *, time_t, time_t *);
void scheduler_free(struct scheduler *);
void scheduler_free_rules(const struct sched_rule * const *);

#endif /* _SCHEDULER_H */
