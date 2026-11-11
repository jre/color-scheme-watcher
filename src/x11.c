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

#include <limits.h>

#include <glib.h>
#include <glib-unix.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_xrm.h>

#include "config.h"
#include "escapes.h"
#include "log.h"
#include "x11.h"

G_STATIC_ASSERT(sizeof(x11_window_t) == sizeof(xcb_window_t));

#define TTY_PROP_NAME		"COLOR_SCHEME_WATCHER_TTY"
static xcb_atom_t tty_prop_atom;

struct cookies {
	xcb_get_property_cookie_t class, tty;
	xcb_query_tree_cookie_t tree;
	xcb_window_t win;
};

/*
  Error meanings taken from:
    https://xorg.freedesktop.org/archive/X11R7.7-RC1/doc/xproto/x11protocol.html
  Only the errors from the following requests are included:
    ChangeProperty, GetProperty, InternAtom, QueryTree
*/
static const char *
errname(uint8_t code)
{
	switch (code) {
	case XCB_ALLOC: return "ALLOC: insufficient resources";
	case XCB_ATOM: return "ATOM: invalid atom";
	case XCB_MATCH: return "MATCH: invalid argument";
	case XCB_VALUE: return "VALUE: value out of range";
	case XCB_WINDOW: return "WINDOW: unknown window";
	default: return "unknown error";
	}
}

static void
check_error(xcb_connection_t *con)
{
	switch (xcb_connection_has_error(con)) {
	case 0:
		return;
	case XCB_CONN_ERROR:
		log_fatal("X11 connection error");
		return;
	case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
		log_fatal("X11 error: extension not supported");
		return;
	case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
		log_fatal("X11 error: no memory");
		return;
	case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
		log_fatal("X11 error: request length exceeded");
		return;
	case XCB_CONN_CLOSED_PARSE_ERR:
		log_fatal("X11 error: invalid $DISPLAY format");
		return;
	case XCB_CONN_CLOSED_INVALID_SCREEN:
		log_fatal("X11 error: invalid screen id");
		return;
	default:
		log_fatal("X11 error: unknown error");
		return;
	}
}

static void
log_x11_error(xcb_connection_t *con, xcb_generic_error_t *err, const char *msg)
{
	log_error("x11 error: %s: %s (%hhu)", msg, errname(err->error_code),
	    err->error_code);
	free(err);
	check_error(con);
}

static gboolean
intern_atom_sync(xcb_connection_t *con, const char *name, xcb_atom_t *atom)
{
	xcb_intern_atom_reply_t *reply;
	xcb_generic_error_t *err;
	xcb_intern_atom_cookie_t ck;

	ck = xcb_intern_atom(con, 0, strlen(name), name);
	err = NULL;
	if ((reply = xcb_intern_atom_reply(con, ck, &err)) == NULL) {
		if (err != NULL)
			log_x11_error(con, err, "InternAtom");
		free(reply);
		return FALSE;
	}
	g_assert(err == NULL);

	*atom = reply->atom;
	free(reply);
	return TRUE;
}

xcb_connection_t *
x11_connect(void)
{
	xcb_connection_t *con;

	if ((con = xcb_connect(NULL, NULL)) == NULL)
		log_fatal("X11 connection failed");
	check_error(con);

	if (!intern_atom_sync(con, TTY_PROP_NAME, &tty_prop_atom)) {
		log_fatal("X11 setup failed");
		xcb_disconnect(con);
		return NULL;
	}

	return con;
}

void
x11_disconnect(xcb_connection_t *con)
{
	xcb_disconnect(con);
}

static void
query_windows(xcb_connection_t *con, xcb_window_t *win, size_t len, GQueue *q)
{
	struct cookies *ck;
	size_t i;

	for (i = 0; i < len; i++) {
		ck = g_new0(struct cookies, 1);
		ck->win = win[i];
		ck->tree = xcb_query_tree(con, win[i]);
		ck->class = xcb_icccm_get_wm_class(con, win[i]);
		ck->tty = xcb_get_property(con, 0, win[i], tty_prop_atom,
		    XCB_ATOM_STRING, 0, PATH_MAX / 4);
		g_queue_push_tail(q, ck);
	}
}

static void
query_children(xcb_connection_t *con, struct cookies *ck, GQueue *q)
{
	xcb_query_tree_reply_t *reply;
	xcb_generic_error_t *err;
	xcb_window_t *children;
	int len;

	err = NULL;
	if ((reply = xcb_query_tree_reply(con, ck->tree, &err)) == NULL) {
		if (err != NULL)
			log_x11_error(con, err, "QueryTree");
		return;
	}
	g_assert(err == NULL);

	len = xcb_query_tree_children_length(reply);
	children = xcb_query_tree_children(reply);
	if (children != NULL && len > 0)
		query_windows(con, children, len, q);
	free(reply);
}

static struct x11_term_info *
validate_window(xcb_connection_t *con, struct cookies *ck)
{
	xcb_icccm_get_wm_class_reply_t rclass;
	xcb_get_property_reply_t *rtty;
	struct x11_term_info *ti;
	xcb_generic_error_t *err;
	enum term_type term_type;
	char *tty_str;
	int tty_len;

	ti = NULL;
	term_type = TERM_UNKNOWN;

	err = NULL;
	if (!xcb_icccm_get_wm_class_reply(con, ck->class, &rclass, &err)) {
		if (err != NULL)
			log_x11_error(con, err, "GetProperty: WM_CLASS");
	} else {
		g_assert(err == NULL);
		term_type = term_class_type(rclass.class_name);
		xcb_icccm_get_wm_class_reply_wipe(&rclass);
	}

	err = NULL;
	if ((rtty = xcb_get_property_reply(con, ck->tty, &err)) == NULL) {
		if (err != NULL)
			log_x11_error(con, err, "GetProperty: " TTY_PROP_NAME);
		return NULL;
	}
	g_assert(err == NULL);

	if (term_type != TERM_UNKNOWN &&
	    rtty->type == XCB_ATOM_STRING && rtty->format == 8 &&
	    (tty_len = xcb_get_property_value_length(rtty)) > 0 &&
	    (tty_str = xcb_get_property_value(rtty)) != NULL) {
		ti = g_new0(struct x11_term_info, 1);
		ti->term_type = term_type;
		ti->tty_path = g_strndup(tty_str, tty_len);
	}
	free(rtty);

	return ti;
}

static void
free_x11_term_info(struct x11_term_info *ti)
{
	if (ti != NULL)
		g_free((char*)ti->tty_path);
	g_free(ti);
}

GList *
x11_find_term_windows(xcb_connection_t *con)
{
	struct x11_term_info *ti;
	xcb_screen_t *screen;
	struct cookies *ck;
	GList *found, *link;
	GQueue queries;

	found = NULL;
	g_queue_init(&queries);
	screen = xcb_setup_roots_iterator(xcb_get_setup(con)).data;
	query_windows(con, &(screen->root), 1, &queries);

	while ((link = g_queue_pop_head_link(&queries)) != NULL) {
		ck = link->data;
		query_children(con, ck, &queries);
		if ((ti = validate_window(con, ck)) == NULL)
			g_free(link);
		else {
			link->data = ti;
			found = g_list_insert_before_link(found, NULL, link);
		}
		g_free(ck);
	}

	return found;
}

void
x11_free_term_windows(GList *list)
{
	g_list_free_full(list, (GDestroyNotify)free_x11_term_info);
}

static xcb_void_cookie_t *
change_property_8bit(xcb_connection_t *con, xcb_window_t win, xcb_atom_t prop,
    xcb_atom_t type, size_t datalen, const void *data, size_t *nret)
{
	size_t maxlen, nreq, off, i;
	xcb_void_cookie_t *ck;
	uint8_t mode;

	maxlen = xcb_get_setup(con)->maximum_request_length -
	    sizeof(xcb_change_property_request_t);
	nreq = (datalen / maxlen) + ((datalen % maxlen) != 0);
	ck = g_new0(xcb_void_cookie_t, nreq);
	mode = XCB_PROP_MODE_REPLACE;

	for (i = 0; i < nreq; i++) {
		off = i * maxlen;
		ck[i] = xcb_change_property(con, mode, win, prop, type, 8,
		    MIN(datalen - off, maxlen), (char*)data + off);
		mode = XCB_PROP_MODE_APPEND;
	}

	*nret = nreq;
	return ck;
}


void
x11_update_resources(xcb_connection_t *con,
    const struct config_pair *res, size_t nres)
{
	xcb_void_cookie_t *ck;
	xcb_generic_error_t *err;
	xcb_xrm_database_t *db;
	xcb_screen_t *screen;
	gboolean grabbed;
	size_t nck, i;
	char *dbstr;

	if ((err = xcb_request_check(con, xcb_grab_server(con))) == NULL)
		grabbed = TRUE;
	else {
		grabbed = FALSE;
		log_x11_error(con, err, "failed to grab server");
	}

	screen = xcb_setup_roots_iterator(xcb_get_setup(con)).data;
	db = xcb_xrm_database_from_resource_manager(con, screen);
	if (db == NULL) {
		log_error("failed to read X11 resource database");
		check_error(con);
		goto ungrab;
	}

	for (i = 0; i < nres; i++)
		xcb_xrm_database_put_resource(&db, res[i].key, res[i].value);

	dbstr = xcb_xrm_database_to_string(db);
	ck = change_property_8bit(con, screen->root, XCB_ATOM_RESOURCE_MANAGER,
	    XCB_ATOM_STRING, strlen(dbstr), dbstr, &nck);

	for (i = 0; i < nck; i++)
		if ((err = xcb_request_check(con, ck[i])) != NULL)
			log_x11_error(con, err,
			    "error setting X RESOURCE_MANAGER proper");

	g_free(ck);
	free(dbstr);
	xcb_xrm_database_free(db);

ungrab:
	if (grabbed &&
	    (err = xcb_request_check(con, xcb_ungrab_server(con))) != NULL)
		log_x11_error(con, err, "failed to ungrab server");
}
