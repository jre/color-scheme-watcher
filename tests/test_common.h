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

#ifndef _TEST_COMMON_H
#define _TEST_COMMON_H

#ifdef __linux__
#  define _GNU_SOURCE
#endif

#include <stdlib.h>

#include <glib.h>

#include "log.h"
#include "util.h"

#define TEST_INIT_COMMON(ac, av, pfx)					\
	const char *__test_prefix = pfx;				\
	const char *__keep_tz = getenv("TEST_KEEP_TZ");;		\
	if (__keep_tz == NULL || strcmp(__keep_tz, "1") != 0)		\
		setenv("TZ", "UTC", 1);					\
	g_test_init(ac, av, NULL);					\
	g_log_set_always_fatal(0);					\
	logging_setup(LOG_MODE_TAP);					\
	logging_set_verbose(TRUE)					\

#define TEST_ADD(t)							\
	do {								\
		char *__n = g_strjoin("/", __test_prefix, #t, NULL);	\
		g_test_add_func(__n, test_##t);				\
		g_free(__n);						\
	} while (0)

#define TEST_ADD_ARRAY(t) do {						\
	void (*__f)(const struct test_##t *) = test_##t;		\
	size_t __i;							\
	for (__i = 0; __i < NITEMS(tests_##t); __i++) {			\
		const struct test_##t *__s = &(tests_##t[__i]);		\
		char *__n = g_strjoin("/",				\
		    __test_prefix, #t, __s->test_name, NULL);		\
		g_test_add_data_func(__n, __s, (GTestDataFunc)__f);	\
		g_free(__n);						\
	}								\
} while (0)

#endif /* _TEST_COMMON_H */
