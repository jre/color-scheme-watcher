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

#include <glib.h>
#include <glib-unix.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_xrm.h>

#include "config.h"
#include "log.h"
#include "ttydb.h"
#include "x11.h"

G_STATIC_ASSERT(sizeof(x11_window_t) == sizeof(xcb_window_t));

struct on_window_destroy_data {
	xcb_connection_t *con;
	struct ttydb *db;
};

/*
  Error meanings taken from:
    https://xorg.freedesktop.org/archive/X11R7.7-RC1/doc/xproto/x11protocol.html#Errors
  Only the errors from the following requests are included:
    https://xorg.freedesktop.org/archive/X11R7.7-RC1/doc/xproto/x11protocol.html#requests:ChangeProperty
    https://xorg.freedesktop.org/archive/X11R7.7-RC1/doc/xproto/x11protocol.html#requests:ChangeWindowAttributes
    https://xorg.freedesktop.org/archive/X11R7.7-RC1/doc/xproto/x11protocol.html#requests:GetProperty
*/
static const char *
errname(uint8_t code)
{
	switch (code) {
	case XCB_ACCESS: return "ACCESS: access denied";
	case XCB_ALLOC: return "ALLOC: insufficient resources";
	case XCB_ATOM: return "ATOM: invalid atom";
	case XCB_COLORMAP: return "COLORMAP: unknown colormap";
	case XCB_CURSOR: return "CURSOR: unknown cursor";
	case XCB_MATCH: return "MATCH: invalid argument";
	case XCB_PIXMAP: return "PIXMAP: unknown pixmap";
	case XCB_VALUE: return "VALUE: invalid value";
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
	log_error("%s: %s (%hhu)", msg, errname(err->error_code),
	    err->error_code);
	free(err);
	check_error(con);
}

xcb_connection_t *
x11_connect(void)
{
	xcb_connection_t *con;

	if ((con = xcb_connect(NULL, NULL)) == NULL)
		log_fatal("X11 connection failed");
	check_error(con);

	return con;
}

void
x11_disconnect(xcb_connection_t *con)
{
	xcb_disconnect(con);
}

static gboolean
on_window_destroy(gint fd G_GNUC_UNUSED, GIOCondition ioc, gpointer user_data)
{
	struct on_window_destroy_data *data = user_data;
	xcb_destroy_notify_event_t *ev;
	xcb_generic_event_t *gev;

	if (ioc & (G_IO_HUP|G_IO_ERR|G_IO_NVAL)) {
		check_error(data->con);
		return FALSE;
	}

	while ((gev = xcb_poll_for_event(data->con)) != NULL) {
		/* note that not masking out the SendEvent bit here
		 * means that events from SendEvent will be ignored */
		if (gev->response_type != XCB_DESTROY_NOTIFY)
			continue;
		ev = (xcb_destroy_notify_event_t *)gev;

		log_debug("DestroyNotify for window %u",
		    (unsigned int)ev->window);
		ttydb_invalidate_window(data->db, ev->window,
		    "terminal window closed");
	}
	check_error(data->con);

	return TRUE;
}

guint
x11_watch_ttydb_windows(xcb_connection_t *con, struct ttydb *db)
{
	struct on_window_destroy_data *data;

	data = g_new0(struct on_window_destroy_data, 1);
	data->con = con;
	data->db = db;

	return g_unix_fd_add_full(G_PRIORITY_DEFAULT,
	    xcb_get_file_descriptor(con), G_IO_IN|G_IO_HUP,
	    on_window_destroy, data, g_free);
}

struct validating {
	struct ttyinfo *ti;
	xcb_get_property_cookie_t ck;
};

void
x11_validate_windows(xcb_connection_t *con, struct ttydb *db)
{
	xcb_icccm_get_wm_class_reply_t reply;
	xcb_generic_error_t *err;
	struct validating *item;
	struct ttyinfo *ti;
	GQueue queue;
	GList *link;

	g_queue_init(&queue);
	while ((ti = ttydb_next_unvalidated(db, &link)) != NULL) {
		log_debug("fetch WM_CLASS for info file %s win=%u tty=%s",
		    ti->info_path, ti->window, ti->tty_path);
		item = g_new0(struct validating, 1);
		item->ti = ti;
		item->ck = xcb_icccm_get_wm_class(con, ti->window);
		link->data = item;
		g_queue_push_tail_link(&queue, link);
	}

	while ((item = g_queue_pop_head(&queue)) != NULL) {
		err = NULL;
		if (xcb_icccm_get_wm_class_reply(con, item->ck, &reply, &err))
			ttydb_validate_ttyinfo(db, item->ti, reply.class_name);
		else {
			ttydb_invalidate_ttyinfo(db, item->ti,
			    "failed to read terminal window class property");
			if (err != NULL && err->error_code != XCB_WINDOW)
				log_x11_error(con, err,
				    "error reading WM_CLASS property");
			free(err);
			check_error(con);
		}
		g_free(item);
	}
}

static xcb_void_cookie_t *
change_property(xcb_connection_t *con, xcb_window_t win, xcb_atom_t prop,
    xcb_atom_t type, uint8_t fmt, size_t datalen, const void *data,
    size_t *nret)
{
	size_t maxlen, nreq, off, i;
	xcb_void_cookie_t *ck;
	uint8_t mode;

	maxlen = (xcb_get_setup(con)->maximum_request_length -
	    sizeof(xcb_change_property_request_t)) & (size_t)~0x3;
	nreq = (datalen / maxlen) + ((datalen % maxlen) != 0);
	ck = g_new0(xcb_void_cookie_t, nreq);
	mode = XCB_PROP_MODE_REPLACE;

	for (i = 0; i < nreq; i++) {
		off = i * maxlen;
		ck[i] = xcb_change_property(con, mode, win, prop,
		    type, fmt, MIN(datalen - off, maxlen), (char*)data + off);
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
	ck = change_property(con, screen->root, XCB_ATOM_RESOURCE_MANAGER,
	    XCB_ATOM_STRING, 8, strlen(dbstr), dbstr, &nck);

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
