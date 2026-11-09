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

#include <signal.h>

#include <glib.h>

#include "log.h"
#include "sigsource.h"

static sig_atomic_t got_sig[NSIG];
static gint handling[NSIG];

struct SigSource {
	GSource source;
	int sig;
};
#define SRC_SIG(s)	(((struct SigSource *)(s))->sig)

static gboolean
sig_action(int sig, void (*func)(int))
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = func;
	sa.sa_flags = SA_RESTART;
	if (sigaction(sig, &sa, NULL) == 0)
		return TRUE;
	log_error_errno("sigaction() failed");
	return FALSE;
}

static void
sig_handler(int sig)
{
	got_sig[sig] = TRUE;
	g_main_context_wakeup(NULL);
}

static gboolean
src_prepare(GSource *source, gint *timeout G_GNUC_UNUSED)
{
	return got_sig[SRC_SIG(source)];
}

static gboolean
src_check(GSource *source)
{
	return got_sig[SRC_SIG(source)];
}

static gboolean
src_dispatch(GSource *source, GSourceFunc func, gpointer data)
{
	got_sig[SRC_SIG(source)] = FALSE;
	return func(data);
}

static void
src_finalize(GSource *source)
{
	sig_action(SRC_SIG(source), SIG_DFL);
	g_atomic_int_set(&(handling[SRC_SIG(source)]), 0);
}

guint
sigsource_add(int sig, GSourceFunc func, gpointer arg, GDestroyNotify notify)
{
	static GSourceFuncs funcs = {
		.prepare = src_prepare,
		.check = src_check,
		.dispatch = src_dispatch,
		.finalize = src_finalize,
	};
	GSource *source;
	guint ret;

	g_return_val_if_fail(sig >= 0 && sig < NSIG, 0);
	if (!g_atomic_int_compare_and_exchange(&(handling[sig]), 0, 1))
		g_return_val_if_reached(0);
	if (!sig_action(sig, sig_handler)) {
		g_atomic_int_set(&(handling[sig]), 0);
		g_return_val_if_reached(0);
	}

	source = g_source_new(&funcs, sizeof(struct SigSource));
	SRC_SIG(source) = sig;
	g_source_set_callback(source, func, arg, notify);
	ret = g_source_attach(source, NULL);
	g_source_unref(source);

	return ret;
}

void
sigsource_remove(guint id)
{
	g_source_destroy(g_main_context_find_source_by_id(NULL, id));
}
