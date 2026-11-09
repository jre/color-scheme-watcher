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

#include <stdlib.h>

#include <glib.h>
#include <gio/gio.h>

#include "color_scheme.h"
#include "datetime.h"
#include "dbus.h"
#include "log.h"
#include "names.h"
#include "notify.h"
#include "parser.h"
#include "scheduler.h"
#include "settings.h"
#include "sigsource.h"

struct options {
	gchar *conf;
	gboolean dump, replace, verbose, version;
};

struct state {
	GDBusConnection *bus;
	struct settings *settings;
	struct scheduler *sched;
	char *config_path;
	guint obj_id, own_id, hup_id, alrm_id;
	gboolean owns_name;
};

static gboolean
reload_config(struct scheduler *sched, const char *filename)
{
	const struct sched_rule * const *rules;
	FILE *fh;

	if ((fh = fopen(filename, "r")) == NULL) {
		log_error_errno("failed to open %s", filename);
		return FALSE;
	} else {
		log_debug("loading config from %s", filename);
		rules = parse_config(fh, filename);
		fclose(fh);
		if (rules == NULL)
			return FALSE;
		scheduler_set_rules(sched, rules);
		return TRUE;
	}
}

static void
reschedule(struct state *state, const char *reason)
{
	enum color_scheme new_scheme;
	time_t next_change;

	new_scheme = scheduler_query(state->sched, &next_change);
	if (new_scheme == SCHEME_UNKNOWN && next_change == -1)
		return;
	if (log_info_enabled()) {
		char buf[DT_CTIME_LEN];
		log_info("chose scheme %s on %s, next at %s",
		    color_scheme_name(new_scheme), reason,
		    dt_ctime(buf, sizeof(buf), next_change));
	}
	settings_set_scheme(state->settings, new_scheme);
	dt_alarm_at(next_change);
}

static void
on_owned(gpointer user_data)
{
	struct state *state = (struct state *)user_data;

	state->owns_name = TRUE;
	notify_ready();
	dbus_emit_scheme(state->bus, settings_get_scheme(state->settings));
}

static void
on_scheme_changed(enum color_scheme scheme, void *arg)
{
	struct state *state = arg;

	log_note("color scheme changed to %s", color_scheme_name(scheme));
	notify_status("color scheme is %s", color_scheme_name(scheme));
	if (state->owns_name)
		dbus_emit_scheme(state->bus, scheme);
}

static gboolean
on_sigalrm(gpointer user_data)
{
	reschedule(user_data, "schedule");
	return G_SOURCE_CONTINUE;
}

static gboolean
on_sighup(gpointer user_data)
{
	struct state *state = (struct state *)user_data;

	notify_reloading();
	log_debug("reloading config on SIGHUP");
	if (reload_config(state->sched, state->config_path))
		reschedule(state, "reload");
	else
		log_debug("failed reload config; schedule unchanged");
	notify_ready();

	return G_SOURCE_CONTINUE;
}

static void
exit_cleanup(int exitval, struct state *state)
{
	notify_stopping();

	if (state->alrm_id != 0)
		sigsource_remove(state->alrm_id);
	if (state->hup_id != 0)
		sigsource_remove(state->hup_id);
	if (state->own_id != 0)
		g_bus_unown_name(state->own_id);
	if (state->obj_id != 0)
		g_dbus_connection_unregister_object(state->bus, state->obj_id);
	if (state->bus != NULL)
		g_object_unref(state->bus);
	if (state->settings != NULL)
		settings_free(state->settings);
	if (state->sched != NULL)
		scheduler_free(state->sched);
	g_free(state->config_path);
	g_free(state);

	exit(exitval);
}

static struct state *
startup(char *config_path, gboolean replace)
{
	struct state *state;

	log_debug("starting version %s with replace=%s",
	    VERSION, (replace ? "true" : "false"));

	state = g_new0(struct state, 1);
	logging_set_fatal_handler((log_fatal_handler_t)exit_cleanup, state);

	state->config_path = config_path;
	state->sched = scheduler_create();
	reload_config(state->sched, state->config_path);
	state->settings = settings_new(on_scheme_changed, state);
	state->bus = dbus_get();
	state->obj_id = dbus_register_object(state->bus,
	    (dbus_scheme_func_t)settings_get_scheme, state->settings);
	state->own_id = dbus_own_name(state->bus, replace, on_owned, state);
	state->hup_id = sigsource_add(SIGHUP, on_sighup, state, NULL);
	state->alrm_id = sigsource_add(SIGALRM, on_sigalrm, state, NULL);
	reschedule(state, "startup");

	return state;
}

static void
dump_schedule(char *config_path)
{
	enum color_scheme prev_scheme, new_scheme;
	struct scheduler *sched;
	time_t now, next_change;
	int year, prev_yday;
	struct tm tm;
	char buf[16];

	sched = scheduler_create();
	if (!reload_config(sched, config_path) ||
	    dt_localtime((now = time(NULL)), &tm) == -1)
		exit(1);

	year = tm.tm_year;
	tm.tm_sec = tm.tm_min = tm.tm_hour = 0;
	tm.tm_mday = 1;
	tm.tm_mon = MONTH_JAN;
	tm.tm_isdst = -1;
	if ((now = mktime(&tm)) == -1)
		log_fatal_errno("mktime() failed");

	prev_scheme = SCHEME_UNKNOWN;
	prev_yday = -1;
	while (tm.tm_year == year) {
		if (prev_yday != tm.tm_yday) {
			prev_yday = tm.tm_yday;
			strftime(buf, sizeof(buf), "%b %e", &tm);
			printf("\n%s %s", buf, color_scheme_name(prev_scheme));
		}
		new_scheme = scheduler_query_at(sched, now, &next_change);
		if (new_scheme != prev_scheme) {
			strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
			printf("  -  %s %s", buf, color_scheme_name(new_scheme));
		}

		now = next_change;
		prev_scheme = new_scheme;
		if (dt_localtime(now, &tm) == -1)
			exit(1);
	}
	putc('\n', stdout);
}

static void
parse_args(int *argc, char ***argv, struct options *opts)
{
	GError *err = NULL;
	GOptionContext *ctx;

	GOptionEntry entries[] = {
		{ "conf", 'c', 0, G_OPTION_ARG_FILENAME, &opts->conf,
		  "Path to the configuration file", "FILE" },
		{ "replace", 'r', 0, G_OPTION_ARG_NONE, &opts->replace,
		  "Replace a running instance", NULL },
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &opts->verbose,
		  "Output debugging messages", NULL },
		{ "version", 'V', 0, G_OPTION_ARG_NONE, &opts->version,
		  "Show program version and exit", NULL },
		{ "dump-schedule", 0, 0, G_OPTION_ARG_NONE, &opts->dump,
		  "Dump the configured schedule for this year", NULL },
		G_OPTION_ENTRY_NULL
	};

	memset(opts, 0, sizeof(*opts));

	ctx = g_option_context_new(NULL);
	g_option_context_set_main_group(ctx,
	    g_option_group_new(NULL, NULL, NULL, opts, NULL));
	g_option_context_add_main_entries(ctx, entries, NULL);
	g_option_context_set_summary(ctx,
	    "An xdg-desktop-portal backend which only implements the desktop\n"
	    "color scheme via gsettings.");
	g_option_context_set_description(ctx,
	    "For more information, see\n"
	    "  man xdg-desktop-portal-schemer\n"
	    "and\n"
	    "  https://flatpak.github.io/xdg-desktop-portal/");

	if (!g_option_context_parse(ctx, argc, argv, &err))
		log_fatal_gerror(err, "command-line parsing failed");
	g_option_context_free(ctx);

	if (opts->conf == NULL)
		opts->conf = g_build_path("/", g_get_user_config_dir(),
		    SCHEMER_DEFAULT_CONF, NULL);
}

int
main(int argc, char *argv[])
{
	struct state *state;
	struct options opts;

	notify_starting();
	parse_args(&argc, &argv, &opts);
	logging_set_verbose(opts.verbose);

	if (opts.version)
		printf("%s\n", VERSION);
	else if (opts.dump)
		dump_schedule(opts.conf);
	else {
		state = startup(opts.conf, opts.replace);
		g_main_loop_run(g_main_loop_new(NULL, FALSE));
		exit_cleanup(0, state);
	}

	g_free(opts.conf);
	return 0;
}
