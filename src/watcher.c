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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <gio/gio.h>

#include "color_scheme.h"
#include "config.h"
#include "datetime.h"
#include "dbus.h"
#include "escapes.h"
#include "log.h"
#include "names.h"
#include "notify.h"
#include "paths.h"
#include "sigsource.h"
#include "x11.h"

enum action {
	ACT_RUN, ACT_ONCE, ACT_RES, ACT_VERS
};

struct options {
	gchar *conf;
	enum action action;
	enum color_scheme scheme;
	gboolean no_autostart, verbose;
};

struct state {
	struct config *conf;
	struct xcb_connection_t *xcon;
	GDBusConnection *bus;
	GDBusProxy *proxy;
	gulong sig_id;
	guint hup_id;
	enum color_scheme scheme;
};

static void
apply_scheme(struct config *conf, struct xcb_connection_t *con,
    enum color_scheme scheme)
{
	const struct config_pair *xres;
	struct x11_term_info *ti;
	GList *windows, *i;
	const char *esc;
	ssize_t res;
	size_t len;
	int fd;

	windows = x11_find_term_windows(con);
	for (i = windows; i != NULL; i = i->next) {
		ti = i->data;
		log_debug("writing escapes for tty=%s term=%s",
		    ti->tty_path, term_type_class(ti->term_type));

		if ((fd = open(ti->tty_path, O_WRONLY)) < 0) {
			log_error_errno("%s: failed to open for writing",
			    ti->tty_path);
			continue;
		}

		esc = config_escapes(conf, scheme, ti->term_type);
		len = strlen(esc);
		while (len > 0) {
			if ((res = write(fd, esc, strlen(esc))) < 0) {
				log_error_errno("%s: failed to write escapes",
				    ti->tty_path);
				break;
			}
			esc += res;
			len -= res;
		}

		close(fd);
	}
	x11_free_term_windows(windows);

	xres = config_resources(conf, scheme, &len);
	x11_update_resources(con, xres, len);
}

static void
change_scheme(enum color_scheme scheme, gpointer user_data)
{
	struct state *state = user_data;

	if (scheme == SCHEME_UNKNOWN)
		return;
	if (scheme == state->scheme) {
		log_debug("color scheme unchanged: %s",
		    color_scheme_name(scheme));
		return;
	}

	notify_status("color scheme is %s", color_scheme_name(scheme));

	log_note("color scheme changed %s -> %s",
	    color_scheme_name(state->scheme), color_scheme_name(scheme));
	state->scheme = scheme;
	apply_scheme(state->conf, state->xcon, scheme);
}

static gboolean
on_sighup(gpointer user_data)
{
	struct state *state = (struct state *)user_data;

	notify_reloading();
	log_debug("reloading config on SIGHUP");
	config_reload(state->conf);
	notify_ready();

	return G_SOURCE_CONTINUE;
}

static void
exit_cleanup(int exitval, struct state *state)
{
	notify_stopping();

	if (state->hup_id != 0)
		sigsource_remove(state->hup_id);
	if (state->sig_id != 0)
		g_signal_handler_disconnect(state->proxy, state->sig_id);
	if (state->proxy != NULL)
		g_object_unref(state->proxy);
	if (state->bus != NULL)
		g_object_unref(state->bus);
	if (state->xcon != NULL)
		x11_disconnect(state->xcon);
	if (state->conf != NULL)
		config_free(state->conf);
	g_free(state);

	exit(exitval);
}

static struct state *
act_startup(struct config *conf, struct options *opts)
{
	enum color_scheme init;
	struct state *state;

	log_debug("starting version %s with autostart=%s",
	    VERSION, (opts->no_autostart ? "false" : "true"));

	state = g_new0(struct state, 1);
	logging_set_fatal_handler((log_fatal_handler_t)exit_cleanup, state);
	state->scheme = SCHEME_UNKNOWN;
	state->conf = conf;
	state->xcon = x11_connect();
	state->bus = dbus_get();
	state->proxy = dbus_get_proxy_sync(state->bus, !opts->no_autostart);
	state->sig_id = dbus_proxy_connect(state->proxy, change_scheme, state);
	state->hup_id = sigsource_add(SIGHUP, on_sighup, state, NULL);

	if ((init = dbus_proxy_scheme_sync(state->proxy)) == SCHEME_UNKNOWN)
		notify_status("no color scheme found");
	else
		change_scheme(init, state);

	notify_ready();

	return state;
}

static void
flock_or_die(void)
{
	struct flock lk;
	char *path;
	int fd, flags;

	path = g_build_path("/", g_get_user_cache_dir(),
	    WATCHER_DEFAULT_LOCK, NULL);
	flags = O_WRONLY|O_CREAT|O_CLOEXEC;
#ifdef O_CLOFORK
	flags |= O_CLOFORK;
#endif
	if ((fd = open(path, flags, 0600)) < 0)
		log_fatal("%s: failed to open lockfile", path);

	memset(&lk, 0, sizeof(lk));
	lk.l_type = F_WRLCK;
	lk.l_whence = SEEK_SET;
	if (fcntl(fd, F_SETLK, &lk) < 0) {
		if (errno != EAGAIN)
			log_fatal("%s: failed to obtain lock", path);
		log_fatal("another copy of %s is already running",
		    g_get_prgname());
	}

	free(path);
}

static void
act_dump_resources(struct config *conf, enum color_scheme scheme)
{
	const struct config_pair *res;
	size_t count, i;

	res = config_resources(conf, scheme, &count);
	for (i = 0; i < count; i++)
		printf("%s: %s\n", res[i].key, res[i].value);
}

static void
act_set_once(struct config *conf, enum color_scheme scheme)
{
	struct xcb_connection_t *con;

	con = x11_connect();
	apply_scheme(conf, con, scheme);
	x11_disconnect(con);
}

static gboolean
parse_source(const char *name, const char *value, gpointer data, GError **err)
{
	struct options *opts = (struct options *)data;
	enum action act;

	if (strcmp(name, "--once") == 0)
		act = ACT_ONCE;
	else if (strcmp(name, "--resources") == 0)
		act = ACT_RES;
	else if (strcmp(name, "-V") == 0 || strcmp(name, "--version") == 0) {
		opts->action = ACT_VERS;
		return TRUE;
	} else {
		g_set_error(err, G_OPTION_ERROR, G_OPTION_ERROR_UNKNOWN_OPTION,
		    "unknown option: %s", name);
		return FALSE;
	}

	if (strcasecmp(value, "default") == 0)
		opts->scheme = SCHEME_DEFAULT;
	else if (strcasecmp(value, "dark") == 0 ||
	    strcasecmp(value, "prefer-dark") == 0 ||
	    strcasecmp(value, "night") == 0)
		opts->scheme = SCHEME_DARK;
	else if (strcasecmp(value, "light") == 0 ||
	    strcasecmp(value, "prefer-light") == 0 ||
	    strcasecmp(value, "day") == 0)
		opts->scheme = SCHEME_LIGHT;
	else if (strcasecmp(value, "current") == 0 ||
	    strcasecmp(value, "desktop") == 0 ||
	    strcasecmp(value, "system") == 0 ||
	    strcasecmp(value, "dbus") == 0)
		opts->scheme = SCHEME_UNKNOWN;
	else {
		g_set_error(err, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		    "unknown color scheme source: %s", value);
		return FALSE;
	}

	opts->action = act;

	return TRUE;
}

static void
fixup_scheme_source(struct options *opts)
{
	GDBusConnection *con;
	GDBusProxy *proxy;

	if (opts->scheme != SCHEME_UNKNOWN)
		return;

	con = dbus_get();
	proxy = dbus_get_proxy_sync(con, !opts->no_autostart);
	opts->scheme = dbus_proxy_scheme_sync(proxy);
	g_object_unref(proxy);
	g_object_unref(con);
	if (opts->scheme == SCHEME_UNKNOWN)
		log_fatal("failed to determine current color scheme");
}

static void
parse_args(int *argc, char ***argv, struct options *opts)
{
	GError *err = NULL;
	GOptionContext *ctx;

	GOptionEntry entries[] = {
		{ "conf", 'c', 0, G_OPTION_ARG_FILENAME, &opts->conf,
		  "Path to the configuration file", "FILE" },
		{ "no-autostart", 0, 0, G_OPTION_ARG_NONE, &opts->no_autostart,
		  "Don't automatically start the desktop portal", NULL },
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &opts->verbose,
		  "Output debugging messages", NULL },
		{ "version", 'V', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
		  parse_source, "Show program version and exit", NULL },
		{ "once", 0, 0, G_OPTION_ARG_CALLBACK, parse_source,
		  "Set color scheme once to SOURCE and exit", "SOURCE" },
		{ "resources", 0, 0, G_OPTION_ARG_CALLBACK, parse_source,
		  "Output X11 resources for SOURCE and exit", "SOURCE" },
		G_OPTION_ENTRY_NULL
	};

	memset(opts, 0, sizeof(*opts));
	opts->action = ACT_RUN;

	ctx = g_option_context_new(NULL);
	g_option_context_set_main_group(ctx,
	    g_option_group_new(NULL, NULL, NULL, opts, NULL));
	g_option_context_add_main_entries(ctx, entries, NULL);
	g_option_context_set_summary(ctx,
	    "Watch for desktop color scheme changes and apply to xterm or rxvt.");
	g_option_context_set_description(ctx,
	    "The SOURCE argument for --once or --resources must be one of:\n"
	    "  current                Desktop environment's current color scheme\n"
	    "  default                The configured default color scheme\n"
	    "  light                  The light color scheme\n"
	    "  dark                   The dark color scheme\n"
	    "\n"
	    "For more information, see\n"
	    "  man color-scheme-watcher");

	if (!g_option_context_parse(ctx, argc, argv, &err))
		log_fatal_gerror(err, "command-line parsing failed");
	g_option_context_free(ctx);

	if (opts->conf == NULL)
		opts->conf = g_build_path("/", g_get_user_config_dir(),
		    WATCHER_DEFAULT_CONF, NULL);
	switch (opts->action) {
	case ACT_RES:
	case ACT_ONCE:
		fixup_scheme_source(opts);
		break;
	case ACT_RUN:
	case ACT_VERS:
		break;
	}
}

int
main(int argc, char *argv[])
{
	struct state *state;
	struct config *conf;
	struct options opts;

	notify_starting();
	parse_args(&argc, &argv, &opts);
	logging_set_verbose(opts.verbose);

	conf = config_new(opts.conf);
	config_reload(conf);
	g_free(g_steal_pointer(&opts.conf));

	switch (opts.action) {
	case ACT_VERS:
		printf("%s\n", VERSION);
		break;
	case ACT_RES:
		act_dump_resources(conf, opts.scheme);
		break;
	case ACT_ONCE:
		act_set_once(conf, opts.scheme);
		break;
	case ACT_RUN:
		flock_or_die();
		state = act_startup(conf, &opts);
		g_main_loop_run(g_main_loop_new(NULL, FALSE));
		exit_cleanup(0, state);
	}

	config_free(conf);
	return 0;
}
