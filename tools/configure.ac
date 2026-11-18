dnl AC_PREREQ([2.61])

AC_INIT([], [])

AC_CONFIG_SRCDIR([src/color_scheme.h])

AC_CONFIG_FILES([auto.mk:mk/auto.mk.in])
AC_CONFIG_HEADERS([autoconfig.h:src/autoconfig.h.in])

AC_PROG_CC

[if test -d /usr/lib/64/pkgconfig; then
    export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}${PKG_CONFIG_PATH:+:}/usr/lib/64/pkgconfig"
fi]

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([GLIB], [glib-2.0 gio-2.0])
PKG_CHECK_MODULES([XCB], [xcb xcb-icccm xcb-xrm])

AC_CHECK_DECLS([timer_create], [AC_CHECK_FUNCS([timer_create])], [], [[
#include <signal.h>
#include <time.h>
]])

AC_OUTPUT
