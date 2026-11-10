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
#include <string.h>

#include <glib.h>

#include "color_scheme.h"
#include "config.h"
#include "escapes.h"
#include "ini.h"
#include "ivars.h"
#include "log.h"
#include "util.h"

#define SECT_DARK		"dark"
#define SECT_DEF		""
#define SECT_LIGHT		"light"
#define SECT_GEN		"general"
#define SECT_RES		"x-resources"

#define SECTKEY_TTY_DIR		SECT_GEN IV_SK_SEP "tty-info-directory"
#define SECTKEY_DEF_SCHEME	SECT_GEN IV_SK_SEP "default-color-scheme"

#define KEY_DEF_HOME		"HOME"

#define KEY_SCHEME_FG		"foreground"
#define KEY_SCHEME_BG		"background"
#define KEY_SCHEME_CURSOR	"cursor"
#define KEY_SCHEME_MOUSE_FG	"mouse-foreground"
#define KEY_SCHEME_MOUSE_BG	"mouse-background"
#define KEY_SCHEME_HILITE_FG	"highlight-foreground"
#define KEY_SCHEME_HILITE_BG	"highlight-background"
#define FMT_SECT_KEY_COLOR	"%s" IV_SK_SEP "color%u"

#define DEFAULT_DEFAULT_SCHEME	(SCHEME_LIGHT)

struct config {
	char *filename;
	char *tty_info_dir;
	enum color_scheme default_scheme;
	char *light_escapes[N_TERM_TYPES], *dark_escapes[N_TERM_TYPES];
	struct config_pair *light_res, *dark_res;
	size_t light_res_count, dark_res_count;
};

struct ini_iter {
	const char *filename;
	struct ivars *iv;
};

static void
reset(struct config *conf)
{
	unsigned int i;

	g_free(conf->tty_info_dir);
	conf->tty_info_dir = NULL;
	conf->default_scheme = DEFAULT_DEFAULT_SCHEME;

	for (i = 0; i < N_TERM_TYPES; i++) {
		g_free(conf->light_escapes[i]);
		g_free(conf->dark_escapes[i]);
	}
	memset(&conf->light_escapes, 0, sizeof(conf->light_escapes));
	memset(&conf->dark_escapes, 0, sizeof(conf->dark_escapes));

	memset(&conf->light_res, 0, sizeof(conf->light_res));
	g_free(conf->light_res);
	conf->light_res = NULL;
	conf->light_res_count = 0;

	memset(&conf->dark_res, 0, sizeof(conf->dark_res));
	g_free(conf->dark_res);
	conf->dark_res = NULL;
	conf->dark_res_count = 0;
}

struct config *
config_new(const char *filename)
{
	struct config *conf;

	conf = g_new0(struct config, 1);
	conf->filename = g_strdup(filename);
	reset(conf);

	return conf;
}

void
config_free(struct config *conf)
{
	reset(conf);
	g_free(conf->filename);
	g_free(conf);
}

static int
add_pair(void *data, const char *section, const char *key,
    const char *value, int line)
{
	struct ini_iter *state = data;

	if (key == NULL) {
		if (strchr(section, IV_SK_SEP_CH) == NULL)
			return 1;
		log_error("%s:%d: %s (%c) character not allowed in name",
		    state->filename, line, IV_SK_SEP_NAME, IV_SK_SEP_CH);
		return 0;
	}

	if (strchr(key, IV_SK_SEP_CH) != NULL) {
		log_error("%s:%d: %s (%c) character not allowed in name",
		    state->filename, line, IV_SK_SEP_NAME, IV_SK_SEP_CH);
		return 0;
	}

	ivars_set_key(state->iv, section, key, value);

	return 1;
}

static struct ivars *
parse(const char *filename)
{
	struct ini_iter state;
	int ret;

	state.filename = filename;
	state.iv = ivars_new();
	ivars_set_key(state.iv, SECT_DEF, KEY_DEF_HOME, g_get_home_dir());

	ret = ini_parse(filename, add_pair, &state);
	switch (ret) {
	case 0:
		return state.iv;
	case -1:
		log_error_errno("%s: failed to open", filename);
		break;
	case -2:
		log_error_errno("failed to allocate memory");
		break;
	default:
		log_error("%s:%d: failed to parse", filename, ret);
		break;
	}

	ivars_free(state.iv);
	return NULL;
}

static void
set_tty_dir(struct config *conf, struct ivars *iv)
{
	ivars_set_default_section(iv, SECT_DEF);
	conf->tty_info_dir = ivars_lookup_sectkey(iv, SECTKEY_TTY_DIR);
	if (conf->tty_info_dir == NULL)
		log_error("%s: missing %s setting",
		    conf->filename, SECTKEY_TTY_DIR);
}

static void
set_def_scheme(struct config *conf, struct ivars *iv)
{
	char *val;

	ivars_set_default_section(iv, SECT_DEF);
	if ((val = ivars_lookup_sectkey(iv, SECTKEY_DEF_SCHEME)) == NULL)
		return;
	conf->default_scheme = color_scheme_parse(val);
	switch (conf->default_scheme) {
	case SCHEME_LIGHT:
	case SCHEME_DARK:
		break;
	case SCHEME_UNKNOWN:
		log_error("%s: invalid %s value: %s",
		    conf->filename, SECTKEY_DEF_SCHEME, val);
		/* FALLTHROUGH */
	default:
		conf->default_scheme = DEFAULT_DEFAULT_SCHEME;
		break;
	}
	g_free(val);
}

static int
rgb(char *str, const char **errmsg) {
	size_t i;

	*errmsg = NULL;
	if (str[0] == '\0')
		return 0;
	else if (str[0] != '#') {
		*errmsg = "RGB value must start with a #";
		return 0;
	}
	for (i = 1; str[i] != '\0'; i++)
		if (!g_ascii_isxdigit(str[i])) {
			*errmsg = "RGB value not hexadecimal";
			return 0;
		}
	if (i != 4 && i != 7) {
		*errmsg = "RGB value must have 3 or 6 digits";
		return 0;
	}
	return 1;
}

static void
set_scheme(struct ivars *iv, const char *sect, struct config_scheme *scheme)
{
	struct { const char *key; const char **val; } kv[] = {
		{ KEY_SCHEME_FG, &scheme->foreground },
		{ KEY_SCHEME_BG, &scheme->background },
		{ KEY_SCHEME_CURSOR, &scheme->cursor },
		{ KEY_SCHEME_MOUSE_FG, &scheme->mouse_foreground },
		{ KEY_SCHEME_MOUSE_BG, &scheme->mouse_background },
		{ KEY_SCHEME_HILITE_FG, &scheme->highlight_foreground },
		{ KEY_SCHEME_HILITE_BG, &scheme->highlight_background },
	};
	unsigned int i;

	ivars_set_default_section(iv, SECT_DEF);

	for (i = 0; i < NITEMS(kv); i++)
		*(kv[i].val) = ivars_lookup_key_v(iv, sect, kv[i].key, rgb);

	for (i = 0; i < CONFIG_NCOLORS; i++) {
		char *sectkey = g_strdup_printf(FMT_SECT_KEY_COLOR, sect, i);
		scheme->color[i] = ivars_lookup_sectkey_v(iv, sectkey, rgb);
		g_free(sectkey);
	}
}

static void
set_escapes(struct ivars *iv, const char *sect, char *(*escapes)[N_TERM_TYPES])
{
	struct config_scheme scheme;
	unsigned int i;

	set_scheme(iv, sect, &scheme);
	for (i = 0; i < N_TERM_TYPES; i++)
		(*escapes)[i] = get_term_escapes(i, &scheme);
}

static int
cmp_res(const void *_a, const void *_b)
{
	const struct config_pair *a = _a;
	const struct config_pair *b = _b;
	return strcmp(a->key, b->key);
}

static struct config_pair *
set_res(struct ivars *iv, const char *def_sect, size_t *countp)
{
	const char *sectkey, *val, *key;
	struct config_pair *pairs;
	struct ivars_iter iter;
	size_t i, count;

	ivars_set_default_section(iv, def_sect);

	count = 0;
	ivars_iter_init(iv, &iter, SECT_RES);
	for (count = 0; ivars_iter_next(&iter, &sectkey, &val); count++)
		; /* no body */
	pairs = g_new0(struct config_pair, count);

	ivars_iter_init(iv, &iter, SECT_RES);
	for (i = 0; ivars_iter_next(&iter, &sectkey, &val); i++) {
		if ((key = strchr(sectkey, IV_SK_SEP_CH)) != NULL)
			key++;
		g_assert(key != NULL);
		pairs[i].key = g_strdup(key);
		pairs[i].value = ivars_expand_sect_sectkey_val(
			iv, SECT_RES, sectkey, val);
	}

	qsort(pairs, count, sizeof(pairs[0]), cmp_res);
	*countp = count;
	return pairs;
}

void
config_reload(struct config *conf)
{
	struct ivars *iv;

	log_debug("reading config from %s", conf->filename);
	if ((iv = parse(conf->filename)) == NULL)
		return;

	reset(conf);
	ivars_set_log_label(iv, SECT_GEN);
	set_tty_dir(conf, iv);
	set_def_scheme(conf, iv);
	ivars_set_log_label(iv, "light tty");
	set_escapes(iv, SECT_LIGHT, &conf->light_escapes);
	ivars_set_log_label(iv, "dark tty");
	set_escapes(iv, SECT_DARK, &conf->dark_escapes);
	ivars_set_log_label(iv, "light resources");
	conf->light_res = set_res(iv, SECT_LIGHT, &conf->light_res_count);
	ivars_set_log_label(iv, "dark resources");
	conf->dark_res = set_res(iv, SECT_DARK, &conf->dark_res_count);
	ivars_set_log_label(iv, NULL);

	ivars_free(iv);
}

const char *
config_tty_info_dir(struct config *conf)
{
	return conf->tty_info_dir;
}

const char *
config_escapes(struct config *conf, enum color_scheme scheme, enum term_type term)
{
	g_assert(term >= 0 && term < N_TERM_TYPES);
	switch (scheme) {
	case SCHEME_LIGHT:
		return conf->light_escapes[term];
	case SCHEME_DARK:
		return conf->dark_escapes[term];
	default:
		return config_escapes(conf, conf->default_scheme, term);
	}
}

const struct config_pair *
config_resources(struct config *conf, enum color_scheme scheme, size_t *count)
{
	switch (scheme) {
	case SCHEME_LIGHT:
		*count = conf->light_res_count;
		return conf->light_res;
	case SCHEME_DARK:
		*count = conf->dark_res_count;
		return conf->dark_res;
	default:
		return config_resources(conf, conf->default_scheme, count);
	}
}
