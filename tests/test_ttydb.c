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

#define unlink(...)	mock_unlink_func(__VA_ARGS__)
#define stat(...)	mock_stat_func(__VA_ARGS__)

#include <stdio.h>

#include "mock_stat.h"
#include "mock_unlink.h"

#include "test_common.h"

/* include the actual C file to test static functions */
#include "ttydb.c"

static struct mock_stat_setup stat_mocks[] = {
	{ "/tty", 0, 0, { .st_mode = 0644 | S_IFCHR } },
	{ "/plain", 0, 0, { .st_mode = 0644 | S_IFREG } },
};

static void
test_unvalidated(void)
{
	struct mock_unlink_setup mu[] = {
		{ "/nonexistent/a", 0, 0 },
		{ "/nonexistent/b", 0, 0 },
		{ "/nonexistent/c", 0, 0 },
		{ "/nonexistent/d", 0, 0 },
	};
	const char *calls[NITEMS(mu)+1] = { 0 };
	struct ttyinfo *ti;
	struct ttydb *db;
	size_t i;

	mock_unlink_setup(mu, NITEMS(mu));
	db = ttydb_new(NULL);
	for (i = 0; i < NITEMS(mu); i++) {
		struct timespec ts;
		calls[i] = mu[i].filename;
		char *path = g_strdup(mu[i].filename);
		ti = ttyinfo_new(&path);
		ti->window = i;
		memset(&ts, 0, sizeof(ts));
		ts.tv_sec = i + 1;
		g_assert_cmpint(i, ==, g_queue_get_length(&db->unvalidated));
		db_add_unvalidated(db, ti, &ts);
	}
	g_assert_cmpint(i, ==, g_queue_get_length(&db->unvalidated));
	for (i = 0; i < NITEMS(mu); i++) {
		ti = ttydb_next_unvalidated(db, NULL);
		g_assert_true(ti != NULL);
		g_assert_cmpint(ti->window, ==, i);
		g_assert_cmpstr(ti->info_path, ==, mu[i].filename);
		g_assert_cmpint(ti->info_ctime.tv_sec, ==, i + 1);
		db_remove_and_unlink_file(db, ti, NULL, "testing");
	}
	g_assert_cmpint(0, ==, g_queue_get_length(&db->unvalidated));
	g_assert_cmpstrv(calls, mock_unlink_get_calls());
	mock_unlink_teardown();
	ttydb_free(db);
}

static void
test_db_needs_update(void)
{
	struct timespec ts;
	struct ttyinfo *ti;
	struct ttydb *db;
	char *path;

	db = ttydb_new(NULL);

	path = g_strdup("/nonexistent/1");
	ti = ttyinfo_new(&path);
	ti->window = 1;
	ti->info_ctime.tv_sec = 12345;
	db_add_validated(db, ti);

	path = g_strdup("/nonexistent/2");
	ti = ttyinfo_new(&path);
	ti->window = 2;
	ti->info_ctime.tv_sec = 67890;
	db_add_validated(db, ti);

	memset(&ts, 0, sizeof(ts));
	ts.tv_sec = 12345;
	g_assert_false(db_needs_update(db, "/nonexistent/1", &ts));
	ts.tv_sec++;
	g_assert_true(db_needs_update(db, "/nonexistent/1", &ts));
	ts.tv_sec = 67890;
	g_assert_false(db_needs_update(db, "/nonexistent/2", &ts));
	ts.tv_sec--;
	g_assert_true(db_needs_update(db, "/nonexistent/2", &ts));

	ttydb_free(db);
}

static void
test_invalidate(void)
{
	struct mock_unlink_setup mu[] = {
		{ "/nonexistent/a", 0, 0 },
		{ "/nonexistent/b", 0, 0 },
		{ "/nonexistent/c", 0, 0 },
		{ "/nonexistent/d", 0, 0 },
	};
	const char *calls[] = {
		"/nonexistent/d", "/nonexistent/a", "/nonexistent/b", NULL
	};
	struct ttydb_iter iter;
	const struct ttyinfo *cti;
	struct ttyinfo *ti;
	struct ttydb *db;
	size_t i;

	mock_unlink_setup(mu, NITEMS(mu));
	db = ttydb_new(NULL);
	for (i = 0; i < NITEMS(mu); i++) {
		char *path = g_strdup(mu[i].filename);
		ti = ttyinfo_new(&path);
		ti->window = i + 1;
		ti->term_type = i;
		g_assert_cmpint(i, ==, g_hash_table_size(db->main_hash));
		db_add_validated(db, ti);
	}
	g_assert_cmpint(i, ==, g_hash_table_size(db->main_hash));

	ttydb_invalidate_ttyinfo(db, ti, "testing");
	g_assert_cmpint(3, ==, g_hash_table_size(db->main_hash));

	ttydb_iter_start(db, &iter);
	while ((cti = ttydb_iter_next(&iter)) != NULL)
		if (cti->term_type == TERM_XTERM)
			ttydb_invalidate_iter(&iter, "testing");
	g_assert_cmpint(2, ==, g_hash_table_size(db->main_hash));

	ttydb_invalidate_window(db, 2, "testing");
	g_assert_cmpint(1, ==, g_hash_table_size(db->main_hash));

	g_assert_cmpstrv(calls, mock_unlink_get_calls());
	mock_unlink_teardown();
	ttydb_free(db);
}

static struct test_ttydb_validate_ttyinfo {
	const char *class; enum term_type type; const char *test_name;
} tests_ttydb_validate_ttyinfo[] = {
	{ "XTerm", TERM_XTERM, "xterm" },
	{ "Rxvt", TERM_RXVT, "rxvt" },
	{ "URxvt", TERM_URXVT, "urxvt" },
	{ "xterm", TERM_UNKNOWN, "lowercase" },
	{ "garbage", TERM_UNKNOWN, "unknown" },
};

static void
test_ttydb_validate_ttyinfo(const struct test_ttydb_validate_ttyinfo *t)
{
	struct mock_unlink_setup mi = { "/nonexistent/file", 0, 0 };
	const char *calls[2] = { 0 };
	struct ttydb *db;
	struct ttyinfo *ti;
	char *path;

	if (t->type == TERM_UNKNOWN)
		calls[0] = mi.filename;
	mock_unlink_setup(&mi, 1);
	db = ttydb_new(NULL);
	path = g_strdup(mi.filename);
	ti = ttyinfo_new(&path);
	ttydb_validate_ttyinfo(db, ti, t->class);
	if (t->type == TERM_UNKNOWN) 
		g_assert_cmpint(g_hash_table_size(db->main_hash), ==, 0);
	else {
		g_assert_cmpint(g_hash_table_size(db->main_hash), ==, 1);
		g_assert_true(ti->term_type == t->type);
	}
	g_assert_cmpstrv(calls, mock_unlink_get_calls());
	mock_unlink_teardown();
	ttydb_free(db);
}

static struct test_trim_ws {
	const char *in; size_t size; size_t ipos; size_t opos;
	const char *expect; const char *test_name;
} tests_trim_ws[] = {
	{ "\0 foo\0 ",       8, 2, 2, "\0 foo\0 ",       "noop" },
	{ "\0   foo\0 \0 ", 12, 2, 4, "\0   foo\0 \0 ",  "front" },
	{ "\0   foo  \0 ",  12, 4, 4, "\0   foo\0\0\0 ", "back" },
	{ "\0   foo  \0 ",  12, 2, 4, "\0   foo\0\0\0 ", "both" },
	{ "\0     \0 ",      9, 2, 6, "\0     \0 ",      "overrun" },
};

static void
test_trim_ws(const struct test_trim_ws *t)
{
	char *buf = g_memdup2(t->in, t->size);
	char *result = trim_ws(buf + t->ipos);
	g_assert_true(result == buf + t->opos);
	g_assert_cmpmem(buf, t->size, t->expect, t->size);
	g_free(buf);
}

static struct test_add_window {
	char *in; gboolean ret; x11_window_t win;
	const char *test_name;
} tests_add_window[] = {
	{ " 23", TRUE, 23, "good" },
	{ "-5", FALSE, 0, "negative" },
	{ "017", TRUE, 17, "not-octal" },
	{ "0x10", FALSE, 0, "not-hex" },
	{ "4294967295", TRUE, 0xffffffffU, "max" },
	{ "4294967296", FALSE, 0, "overflow" },
};

static void
test_add_window(const struct test_add_window *t)
{
	struct ttyinfo out, expect;
	gboolean res;

	memset(&expect, 0xc3, sizeof(expect));
	expect.info_path = "/nonexistent/fake";
	out = expect;
	if (t->ret)
		expect.window = t->win;
	res = add_window(&out, 0, t->in);
	g_assert_true(res == t->ret);
	g_assert_cmpmem(&out, sizeof(out), &expect, sizeof(expect));
}

static const char *st_null[] = { NULL };
static const char *st_tty[] = { "/tty", NULL };
static const char *st_nope[] = { "/nope", NULL };
static const char *st_plain[] = { "/plain", NULL };
static const char *st_pl_tty[] = { "/plain", "/tty", NULL };
static struct test_ttyinfo_parse {
	const char *data; gboolean good; const char *tty; x11_window_t win;
	const char **calls; const char *test_name;
} tests_ttyinfo_parse[] = {
	{ "win=1\ntty=/tty\n", TRUE, "/tty", 1, st_tty, "nominal" },
	{ " tty=\t/tty\n  \nwin =2 ", TRUE, "/tty", 2, st_tty, "spacy" },
	{ "win=3\ntty=/tty\ntty=/plain", TRUE, "/tty", 3, st_tty, "overwrite" },
	{ "win=4\ntty=/nope", FALSE, NULL, 0, st_nope, "nonexistent" },
	{ "win=5\ntty=/plain", FALSE, NULL, 0, st_plain, "not-a-tty" },
	{ "win=6\ntty=/plain\ntty=/tty", TRUE, "/tty", 6, st_pl_tty, "tty2" },
	{ "tty=/tty", FALSE, NULL, 0, st_tty, "no-window" },
	{ "foo=bar\nwin=7\ntty=/tty", TRUE, "/tty", 7, st_tty, "unknown-key" },
	{ "=bar\nwin=8\ntty=/tty", TRUE, "/tty", 8, st_tty, "no-key" },
	{ "foo=\nwin=9\ntty=/tty", TRUE, "/tty", 9, st_tty, "no-val" },
	{ "WIN=10\nTTY=/tty\n", TRUE, "/tty", 10, st_tty, "CAPS" },
	{ "", FALSE, NULL, 0, st_null, "empty" },
};

static void
test_ttyinfo_parse(const struct test_ttyinfo_parse *t)
{
	char *path, *opath, *buf;
	struct ttyinfo *ti;

	mock_stat_setup(stat_mocks, NITEMS(stat_mocks));
	path = g_strconcat("/nonexistent/", t->test_name, NULL);
	opath = path;
	buf = g_strdup(t->data);
	ti = ttyinfo_parse(&path, buf);
	g_free(buf);
	if (t->good) {
		g_assert_true(ti != NULL);
		g_assert_true(path == NULL);
		g_assert_true(ti->info_path == opath);
		g_assert_cmpstr(ti->tty_path, ==, t->tty);
		g_assert_cmpint(ti->window, ==, t->win);
		g_assert_true(ti->term_type == TERM_UNKNOWN);
		ttyinfo_free(ti);
	} else {
		g_assert_true(ti == NULL);
		g_free(path);
	}
	g_assert_cmpstrv(t->calls, mock_stat_get_calls());
	mock_stat_teardown();
}

int
main(int argc, char *argv[])
{
	TEST_INIT_COMMON(&argc, &argv, "/ttydb");

	TEST_ADD(unvalidated);
	TEST_ADD(db_needs_update);
	TEST_ADD(invalidate);
	TEST_ADD_ARRAY(ttydb_validate_ttyinfo);
	TEST_ADD_ARRAY(trim_ws);
	TEST_ADD_ARRAY(add_window);
	TEST_ADD_ARRAY(ttyinfo_parse);

	return g_test_run();
}
