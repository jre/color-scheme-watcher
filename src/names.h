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

#ifndef _NAMES_H
#define _NAMES_H

#define BUS_NAME_PUB	"org.freedesktop.portal.Desktop"
#define IFACE_NAME_PUB	"org.freedesktop.portal.Settings"
#define BUS_NAME_IMPL	"org.freedesktop.impl.portal.desktop.schemer"
#define IFACE_NAME_IMPL	"org.freedesktop.impl.portal.Settings"
#define OBJ_PATH	"/org/freedesktop/portal/desktop"
#define SETTINGS_NS	"org.freedesktop.appearance"
#define SETTINGS_KEY	"color-scheme"
#define CHANGED_SIG	"SettingChanged"

#define SCHEMER_DEFAULT_CONF	"xdg-desktop-portal-schemer.conf"
#define WATCHER_DEFAULT_CONF	"color-scheme-watcher.ini"
#define WATCHER_DEFAULT_LOCK	".color-scheme-watcher.lock"

#endif /* _NAMES_H */
