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

#include "ivars.h"
#include "log.h"

#define VAR_DELIM		'$'
#define VAR_OPEN		'{'
#define VAR_CLOSE		'}'

struct ivars {
	GHashTable *vars, *cycle;
	const char *def_sect, *label;
};

static char *lookup_sect_anykey(struct ivars *, const char *, const char *);
static char *lookup_sect_sectkey(struct ivars *, const char *, const char *);

struct ivars *
ivars_new(void)
{
	struct ivars *iv;

	iv = g_new0(struct ivars, 1);
	/* XXX case-insensitive but preserving would be nice */
	iv->vars = g_hash_table_new_full(g_str_hash, g_str_equal,
	    g_free, g_free);
	iv->cycle = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

	return iv;
}

void
ivars_free(struct ivars *iv)
{
	g_hash_table_destroy(iv->vars);
	g_hash_table_destroy(iv->cycle);
	g_free(iv);
}

void
ivars_set_default_section(struct ivars *iv, const char *sect)
{
	iv->def_sect = sect;
}

void
ivars_set_log_label(struct ivars *iv, const char *label)
{
	iv->label = label;
}

static char *
sectkey_dup_sect(const char *sectkey) {
	const char *sep;

	if ((sep = strchr(sectkey, IV_SK_SEP_CH)) == NULL)
		return NULL;

	return g_strndup(sectkey, sep - sectkey);
}

static char *
sect_key_join(const char *sect, const char *key)
{
	return g_strjoin(IV_SK_SEP, sect, key, NULL);
}

void
ivars_set_key(struct ivars *iv, const char *sect,
    const char *key, const char *val)
{
	g_hash_table_replace(iv->vars, sect_key_join(sect, key),
	    g_strdup(val));
}

static void
cycle_add_sectkey(struct ivars *iv, const char *sectkey)
{
	g_hash_table_add(iv->cycle, (void *)sectkey);
}

static void
lookup_done(struct ivars *iv)
{
	g_hash_table_remove_all(iv->cycle);
}

static char *
append_str(char *str, const char *start, const char *end)
{
	size_t old_size, new_size;

	if (end == NULL)
		end = strchr(start, '\0');

	if (str == NULL) {
		old_size = 0;
		new_size = (end - start) + 1;
	} else {
		old_size = strlen(str) + 1;
		new_size = old_size + (end - start);
	}

	str = g_realloc(str, new_size);
	str[old_size] = '\0';
	g_strlcat(str, start, new_size);

	return str;
}

static char *
expand_sect_sectkey_val(struct ivars *iv, const char *sect,
    const char *sectkey, const char *rawval)
{
	const char *copy_start, *copy_end, *scan_at;
	char *res;

	res = NULL;
	copy_start = scan_at = rawval;
	while (*scan_at != '\0') {
		const char *delim, *vstart, *vend;
		char *refname, *refval;

		copy_end = vstart = vend = NULL;
		delim = strchr(scan_at, VAR_DELIM);

		/* this block must advance scan_at */
		if (delim == NULL)
			/* no $ found before of end of string */
			scan_at = strchr(scan_at, '\0'); /* copy and stop */
		else if (delim[1] == VAR_DELIM) {
			/* found $$ sequence, convert to one $ char */
			copy_end = delim + 1;	/* copy to after first $ */
			scan_at = delim + 2;	/* scan after second $ */
		} else if (delim[1] == '\0') {
			/* found $ at end of string */
			scan_at = delim + 1;	/* copy and stop */
			log_warn("%s: found %c at end of line",
			    sectkey, VAR_DELIM);
		} else if (delim[1] != VAR_OPEN) {
			/* found $ followed by non { char, skip over */
			scan_at = delim + 2;	/* scan at $ + 2 */
			log_warn("%s: found %c not followed by %c or %c",
			    sectkey, VAR_DELIM, VAR_DELIM, VAR_OPEN);
		} else if ((vend = strchr(delim + 2, VAR_CLOSE)) == NULL) {
			/* no } found, skip over ${ */
			scan_at = delim + 2;	/* scan at $ + 2 */
			log_warn("%s: found %c%c without %c",
			    sectkey, VAR_DELIM, VAR_OPEN, VAR_CLOSE);
		} else {
			/* found valid ${name} sequence */
			copy_end = delim;
			if (delim + 2 < vend)
				/* don't set scan_at here to avoid
				 * resetting copy_end below */
				vstart = delim + 2;
			else {
				scan_at = vend + 1;
				log_warn("%s: found %c%c%c",
				    sectkey, VAR_DELIM, VAR_OPEN, VAR_CLOSE);
			}
		}

		/* scanning at end of string means it's time to copy */
		if (*scan_at == '\0')
			copy_end = scan_at;

		/* copy into the result string if needed */
		if (copy_end != NULL && copy_start < copy_end) {
			res = append_str(res, copy_start, copy_end);
			copy_start = scan_at;
		}

		/* look up variable reference and append if needed */
		if (vstart == NULL)
			continue;
		refname = append_str(NULL, vstart, vend);
		refval = lookup_sect_anykey(iv, sect, refname);
		if (refval == NULL)
			log_warn("%s: reference to unknown key %s",
			    sectkey, refname);
		else
			res = append_str(res, refval, NULL);
		g_free(refval);
		g_free(refname);
		copy_start = scan_at = vend + 1;
	}

	return res;
}

static char *
lookup_sect_anykey(struct ivars *iv, const char *sect, const char *anykey)
{
	const char *sep;
	char *sectkey, *val;

	if ((sep = strchr(anykey, IV_SK_SEP_CH)) != NULL)
		return lookup_sect_sectkey(iv, sect, anykey);

	if (sect != NULL) {
		sectkey = sect_key_join(sect, anykey);
		val = lookup_sect_sectkey(iv, sect, sectkey);
		g_free(sectkey);
		if (val != NULL)
			return val;
	}

	if (iv->def_sect != NULL) {
		sectkey = sect_key_join(iv->def_sect, anykey);
		val = lookup_sect_sectkey(iv, iv->def_sect, sectkey);
		g_free(sectkey);
	}

	return val;
}

static char *
lookup_sect_sectkey(struct ivars *iv, const char *sect, const char *sectkey)
{
	const char *rawval;

	if ((rawval = g_hash_table_lookup(iv->vars, sectkey)) == NULL)
		return NULL;

	cycle_add_sectkey(iv, sectkey);
	return expand_sect_sectkey_val(iv, sect, sectkey, rawval);
}

static void
log_lookup(struct ivars *iv, const char *sectkey,
    const char *rawval, const char *newval)
{
	const char *label;

	label = (iv->label == NULL ? "" : iv->label);
	if (newval == NULL) {
		log_debug("config %s val %s unset", label, sectkey);
		return;
	}
	if (rawval == NULL)
		rawval = g_hash_table_lookup(iv->vars, sectkey);
	if (strcmp(rawval, newval) == 0)
		log_debug("config %s val %s is \"%s\"",
		    label, sectkey, newval);
	else
		log_debug("config %s val %s expanded \"%s\" -> \"%s\"",
		    label, sectkey, rawval, newval);
}

char *
ivars_expand_sect_sectkey_val(struct ivars *iv, const char *sect,
    const char *sectkey, const char *rawval)
{
	char *ret;

	cycle_add_sectkey(iv, sectkey);
	ret = expand_sect_sectkey_val(iv, sect, sectkey, rawval);
	if (log_debug_enabled())
		log_lookup(iv, sectkey, rawval, ret);
	lookup_done(iv);

	return ret;
}

char *
ivars_lookup_key_v(struct ivars *iv, const char *sect, const char *key,
    int (*validate)(char *, const char **))
{
	char *sectkey, *ret;
	const char *errmsg;

	sectkey = sect_key_join(sect, key);
	cycle_add_sectkey(iv, sectkey);
	ret = lookup_sect_sectkey(iv, sect, sectkey);
	if (!validate(ret, &errmsg)) {
		if (errmsg != NULL)
			log_error("%s: %s", sectkey, errmsg);
		g_free(ret);
		ret = NULL;
	}
	if (log_debug_enabled())
		log_lookup(iv, sectkey, NULL, ret);
	lookup_done(iv);
	g_free(sectkey);

	return ret;
}

char *
ivars_lookup_sectkey(struct ivars *iv, const char *sectkey)
{
	char *sect, *ret;

	sect = sectkey_dup_sect(sectkey);
	cycle_add_sectkey(iv, sectkey);
	ret = lookup_sect_sectkey(iv, sect, sectkey);
	if (log_debug_enabled())
		log_lookup(iv, sectkey, NULL, ret);
	lookup_done(iv);
	g_free(sect);

	return ret;
}

char *
ivars_lookup_sectkey_v(struct ivars *iv, const char *sectkey,
    int (*validate)(char *, const char **))
{
	const char *errmsg;
	char *ret;

	ret = ivars_lookup_sectkey(iv, sectkey);
	if (!validate(ret, &errmsg)) {
		if (errmsg != NULL)
			log_error("%s: %s", sectkey, errmsg);
		g_free(ret);
		ret = NULL;
	}
	return ret;
}

void
ivars_iter_init(struct ivars *iv, struct ivars_iter *iter, const char *sect)
{
	g_hash_table_iter_init(&iter->_iter, iv->vars);
	iter->_sect = sect;
}

gboolean
ivars_iter_next(struct ivars_iter *iter, const char **key, const char **val)
{
	gpointer kp, vp;
	const char *sep;

	while (g_hash_table_iter_next(&iter->_iter, &kp, &vp)) {
		if (iter->_sect != NULL &&
		    ((sep = strchr(kp, IV_SK_SEP_CH)) == NULL ||
			sep == kp ||
			strncmp(kp, iter->_sect, sep - (char*)kp) != 0))
			continue;

		*key = kp;
		*val = vp;
		return TRUE;
	}

	return FALSE;
}
