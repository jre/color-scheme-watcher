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

#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <glib.h>

#include "log.h"
#include "ttydb.h"
#include "util.h"
#include "x11.h"

G_STATIC_ASSERT(((x11_window_t)~0)
    == GPOINTER_TO_INT(GINT_TO_POINTER(((x11_window_t)~0))));

#define DB_MAIN_ADD(d, t)	g_hash_table_replace((d)->main_hash, \
	    GINT_TO_POINTER((t)->window), (t))
#define DB_MAIN_GET(d, w)	g_hash_table_lookup((d)->main_hash, \
	    GINT_TO_POINTER(w))
#define DB_MAIN_DEL(d, t)	g_hash_table_remove((d)->main_hash, \
	    GINT_TO_POINTER((t)->window))

#define DB_FILENAME_ADD(d, t)	g_hash_table_replace((d)->filename_hash, \
	    (t)->info_path, (t))
#define DB_FILENAME_GET(d, f)	g_hash_table_lookup((d)->filename_hash, (f))
#define DB_FILENAME_DEL(d, t)	g_hash_table_remove((d)->filename_hash, \
	    (t)->info_path)

struct ttydb {
	GHashTable *main_hash, *filename_hash;
	GQueue unvalidated;
	guint window_watcher;
};

static struct { const char *name; enum term_type type; } term_types[] = {
	{ "Rxvt", TERM_RXVT },
	{ "URxvt", TERM_URXVT },
	{ "XTerm", TERM_XTERM },
};

static struct ttyinfo *
ttyinfo_new(char **info_path)
{
	struct ttyinfo *ti;

	ti = g_new0(struct ttyinfo, 1);
	ti->info_path = g_steal_pointer(info_path);
	ti->term_type = TERM_UNKNOWN;

	return ti;
}

static void
ttyinfo_free(struct ttyinfo *ti)
{
	g_free(ti->tty_path);
	g_free(ti->info_path);
	g_free(ti);
}

struct ttydb *
ttydb_new(struct xcb_connection_t *con)
{
	struct ttydb *db;

	db = g_new0(struct ttydb, 1);
	db->main_hash = g_hash_table_new_full(g_direct_hash, g_direct_equal,
	    NULL, (GDestroyNotify)ttyinfo_free);
	db->filename_hash = g_hash_table_new(g_str_hash, g_str_equal);
	g_queue_init(&db->unvalidated);
	if (con != NULL)
		db->window_watcher = x11_watch_ttydb_windows(con, db);

	return db;
}

#if 0
static void
dumpdb(struct ttydb *db)
{
	GHashTableIter iter;
	struct ttyinfo *ti;
	gpointer key, val;

	log_debug("dumping window db:");
	g_hash_table_iter_init(&iter, db->main_hash);
	while (g_hash_table_iter_next(&iter, &key, &val)) {
		ti = val;
		log_debug("%u: %p info=%s win=%u tty=%s",
		    GPOINTER_TO_INT(key), val,
		    ti->info_path, ti->window, ti->tty_path);
	}

	log_debug("dumping filename db:");
	g_hash_table_iter_init(&iter, db->filename_hash);
	while (g_hash_table_iter_next(&iter, &key, &val)) {
		ti = val;
		log_debug("%s: %p info=%s win=%u tty=%s",
		    (char*)key, val, ti->info_path, ti->window, ti->tty_path);
	}
}
#endif

void
ttydb_free(struct ttydb *db)
{
	if (db->window_watcher != 0)
		g_source_remove(db->window_watcher);
	g_assert(g_queue_is_empty(&db->unvalidated));
	g_hash_table_destroy(db->filename_hash);
	g_hash_table_destroy(db->main_hash);
	g_free(db);
}

static void
db_add_unvalidated(struct ttydb *db, struct ttyinfo *ti,
    const struct timespec *ctime)
{
	ti->info_ctime = *ctime;
	g_queue_push_tail(&db->unvalidated, ti);
}

struct ttyinfo *
ttydb_next_unvalidated(struct ttydb *db, GList **linkp)
{
	if (linkp == NULL)
		return g_queue_pop_head(&db->unvalidated);
	if ((*linkp = g_queue_pop_head_link(&db->unvalidated)) == NULL)
		return NULL;
	return g_steal_pointer(&((*linkp)->data));
}

static gboolean
db_needs_update(struct ttydb *db, const char *filename,
    const struct timespec *ctime)
{
	struct ttyinfo *ti;

	if ((ti = DB_FILENAME_GET(db, filename)) == NULL) {
		log_debug("info file %s not in ttydb", filename);
		return TRUE;
	}
	if (timespeccmp(&ti->info_ctime, ctime, !=)) {
		log_debug("info file %s ctime %lld.%.9ld != %lld.%.9ld",
		    ti->info_path, (long long)ti->info_ctime.tv_sec,
		    (long)ti->info_ctime.tv_nsec, (long long)ctime->tv_sec,
		    (long)ctime->tv_nsec);
		return TRUE;
	}
	return FALSE;
}

static void
db_remove_and_unlink_file(struct ttydb *db, struct ttyinfo *ti,
    struct ttydb_iter *iter, const char *msg)
{
	log_debug("%s: removing tty info file: %s", ti->info_path, msg);
	unlink(ti->info_path);
	DB_FILENAME_DEL(db, ti);
	if (iter != NULL) {
		g_hash_table_iter_remove(&iter->_iter);
		iter->_cur = NULL;
	} else if (!DB_MAIN_DEL(db, ti))
		ttyinfo_free(ti);
}

static void
db_add_validated(struct ttydb *db, struct ttyinfo *ti)
{
	struct ttyinfo *dup;

	log_debug("validated tty info file %s with win=%u tty=%s as %s",
	    ti->info_path, ti->window, ti->tty_path,
	    term_type_name(ti->term_type));

	if ((dup = DB_MAIN_GET(db, ti->window)) != NULL) {
		log_error("duplicate info files %s and %s for window %u",
		    dup->info_path, ti->info_path, ti->window);
		if (timespeccmp(&dup->info_ctime, &ti->info_ctime, >)) {
			db_remove_and_unlink_file(db, ti, NULL, "duplicate");
			return;
		}
		db_remove_and_unlink_file(db, dup, NULL, "duplicate");
	}

	DB_MAIN_ADD(db, ti);
	DB_FILENAME_ADD(db, ti);
}

void
ttydb_iter_start(struct ttydb *db, struct ttydb_iter *iter)
{
	iter->_db = db;
	iter->_cur = NULL;
	g_hash_table_iter_init(&iter->_iter, db->main_hash);
}

const struct ttyinfo *
ttydb_iter_next(struct ttydb_iter *iter)
{
	gpointer val;

	if (!g_hash_table_iter_next(&iter->_iter, NULL, &val))
		val = NULL;
	iter->_cur = val;
	return val;
}

void
ttydb_invalidate_iter(struct ttydb_iter *iter, const char *msg)
{
	db_remove_and_unlink_file(iter->_db, iter->_cur, iter, msg);
}

void
ttydb_invalidate_ttyinfo(struct ttydb *db, struct ttyinfo *ti, const char *msg)
{
	db_remove_and_unlink_file(db, ti, NULL, msg);
}

void
ttydb_invalidate_window(struct ttydb *db, x11_window_t win, const char *msg)
{
	struct ttyinfo *ti;
	if ((ti = DB_MAIN_GET(db, win)) != NULL)
		db_remove_and_unlink_file(db, ti, NULL, msg);
}

void
ttydb_validate_ttyinfo(struct ttydb *db, struct ttyinfo *ti, const char *class)
{
	size_t i;

	for (i = 0; i < NITEMS(term_types); i++)
		if (strcmp(class, term_types[i].name) == 0) {
			ti->term_type = term_types[i].type;
			db_add_validated(db, ti);
			return;
		}
	db_remove_and_unlink_file(db, ti, NULL,
	    "unknown terminal window class property");
}

static char *
trim_ws(char *str)
{
	char *end;

	while (g_ascii_isspace(*str))
		str++;
	end = strchr(str, '\0') - 1;
	while (end >= str && g_ascii_isspace(*end))
		*(end--) = '\0';

	return str;
}

static void
add_tty(struct ttyinfo *ti, int line, const char *val)
{
	struct stat sb;

	if (stat(val, &sb) < 0)
		log_error_errno("%s:%d: stat failed for %s",
		    ti->info_path, line, val);
	else if (!S_ISCHR(sb.st_mode))
		log_error("%s:%d: not a char device: %s",
		    ti->info_path, line, val);
	else
		ti->tty_path = g_strdup(val);
}

static gboolean
add_window(struct ttyinfo *ti, int line, const char *val)
{
	STR_TO_INT_ELSE(val, x11_window_t, ti->window, {
		log_warn("%s:%d: invalid window id: %s",
		    ti->info_path, line, val);
		return FALSE;
	});
	return TRUE;
}

static struct ttyinfo *
ttyinfo_parse(char **path, char *buf)
{
	char *start, *end, *key, *val;
	struct ttyinfo *ti;
	gboolean good_win;
	int lineno;

	ti = ttyinfo_new(path);
	good_win = FALSE;
	lineno = 0;
	for (start = buf; start != NULL && *start; start = end) {
		lineno++;
		if ((end = strchr(start, '\n')) != NULL)
			*(end++) = '\0';
		if ((val = strchr(start, '=')) == NULL) {
			log_warn("%s:%d: syntax_error: missing = character",
			    ti->info_path, lineno);
			continue;
		}
		*(val++) = '\0';

		if (*(key = trim_ws(start)) == '\0') {
			log_warn("%s:%d: syntax error: empty key",
			    ti->info_path, lineno);
			continue;
		}
		if (*(val = trim_ws(val)) == '\0') {
			log_warn("%s:%d: syntax error: empty value",
			    ti->info_path, lineno);
			continue;
		}

		if (ti->tty_path == NULL && strcasecmp(key, "tty") == 0)
			add_tty(ti, lineno, val);
		else if (!good_win && strcasecmp(key, "win") == 0)
			good_win = add_window(ti, lineno, val);
		else
			log_warn("%s:%d: unexpected key: %s",
			    ti->info_path, lineno, key);
	}

	if (good_win && ti->tty_path != NULL)
		return ti;
	*path = g_steal_pointer(&(ti->info_path));
	ttyinfo_free(ti);
	return NULL;
}

static void
load_info_file(struct ttydb *db, char **path)
{
	struct ttyinfo *ti;
	struct stat sb;
	GError *err;
	gchar *data;
	gsize len;

	if (stat(*path, &sb) < 0) {
		log_error_errno("%s: stat failed", *path);
		return;
	}
	if (!db_needs_update(db, *path, &sb.st_ctim))
		return;
	if (!S_ISREG(sb.st_mode)) {
		log_warn("%s: ignoring non-regular file", *path);
		return;
	}

	data = NULL;
	err = NULL;
	if (!g_file_get_contents(*path, &data, &len, &err)) {
		log_error_gerror(err, "error reading file %s", *path);
		return;
	}

	ti = ttyinfo_parse(path, data);
	g_free(data);
	if (ti == NULL) {
		log_debug("%s: removing bad tty info file", *path);
		unlink(*path);
		return;
	}

	log_debug("read tty info file %s with win=%u tty=%s",
	    ti->info_path, (unsigned int)ti->window, ti->tty_path);
	db_add_unvalidated(db, ti, &sb.st_ctim);
}

void
ttydb_reload(struct ttydb *db, struct xcb_connection_t *con,
    const char *dirpath)
{
	const char *filename;
	gchar *filepath;
	GError *err;
	GDir *dir;

	if (dirpath == NULL)
		return;

	err = NULL;
	if ((dir = g_dir_open(dirpath, 0, &err)) == NULL) {
		log_error_gerror(err, "error reading %s directory", dirpath);
		return;
	}

	while ((filename = g_dir_read_name(dir)) != NULL) {
		if (filename[0] == '.')
			log_warn("%s: ignoring temp file", filename);
		else {
			filepath = g_build_path("/", dirpath, filename, NULL);
			load_info_file(db, &filepath);
			g_free(filepath);
		}
	}

	g_dir_close(dir);

	x11_validate_windows(con, db);
}

const char *
term_type_name(enum term_type type)
{
	size_t i;

	for (i = 0; i < NITEMS(term_types); i++)
		if (term_types[i].type == type)
			return term_types[i].name;
	return "unknown";
}
