SRCDIR = ${TOP}/src
TOOLDIR = ${TOP}/tools
TESTDIR = ${TOP}/tests
OBJDIR ?= ${TOP}/build

PREFIX ?= /usr/local
ETCDIR ?= /etc
UNITDIR ?= ${ETCDIR}/systemd/user
BINDIR ?= ${PREFIX}/bin
LIBEXECDIR ?= ${PREFIX}/libexec
MANDIR ?= ${PREFIX}/man
MAN1DIR ?= ${MANDIR}/man1
MAN5DIR ?= ${MANDIR}/man5
SHAREDIR ?=  ${PREFIX}/share
PORTALDIR ?= ${SHAREDIR}/xdg-desktop-portal/portals
DBUSDIR ?= ${SHAREDIR}/dbus-1/services

RECURSE_TARGETS = all depend check test regress tests build-tests \
	install install-watcher install-schemer \
	distribute dist ${PROGS} ${TEST_NAMES}
PHONY_TARGETS = ${RECURSE_TARGETS} clean

CC ?= cc
CFLAGS ?= -O2
CFLAGS += -Wall -Wextra
CFLAGS += -Wno-type-limits
CFLAGS += -Werror
CFLAGS += -g
CFLAGS += -iquote ${SRCDIR} -iquote ${OBJDIR} ${GLIB_CFLAGS}
LDFLAGS ?=
PCC_FLAGS = --lines

DEPFILE = .depend
VERSION_FILE = version.txt
TEST_LOG = ${OBJDIR}/test-suite.log
DIST_NAME = color-scheme-watcher-${DIST_VERSION}
DIST_EXT = tar.gz
NAME_schemer = xdg-desktop-portal-schemer
NAME_watcher = color-scheme-watcher
DBUS_SERVICE = org.freedesktop.impl.portal.desktop.schemer.service

INSTALL_FLAGS ?= -o 0 -g 0
INSTALL_BIN = install ${INSTALL_FLAGS} -m 555
INSTALL_DATA = install ${INSTALL_FLAGS} -m 444
INSTALL_DIR = install -d ${INSTALL_FLAGS} -m 755

PROGS = schemer watcher pcc
TESTS = datetime parser scheduler
PEG = grammar
TESTRUNNER = ${TOOLDIR}/tap-driver.sh

LDFLAGS_TESTS = ${LDFLAGS} ${GLIB_LDFLAGS}
TEST_DEPS_ALL = log.c
TEST_DEPS_datetime = datetime.c
TEST_DEPS_parser = ${PEG}.c color_scheme.c datetime.c parser.c scheduler.c
TEST_DEPS_scheduler = color_scheme.c datetime.c

CFLAGS_MAIN = -DVERSION='"${NAME_$*} ${VERSION}"'
CFLAGS_depend = ${XCB_CFLAGS}
CFLAGS_packcc = -Wno-unused-parameter
CFLAGS_${PEG} = -Wno-unused-parameter
CFLAGS_config = -DINI_HANDLER_LINENO=1
CFLAGS_ini = -DINI_HANDLER_LINENO=1 -DINI_USE_STACK=0 -DINI_ALLOW_REALLOC=1 \
	-DINI_CALL_HANDLER_ON_NEW_SECTION=1
CFLAGS_schemer = ${CFLAGS_MAIN}
CFLAGS_watcher = ${CFLAGS_MAIN}
CFLAGS_x11 = ${XCB_CFLAGS}

LDFLAGS_schemer = ${GLIB_LDFLAGS}
LDFLAGS_watcher = ${GLIB_LDFLAGS} ${XCB_LDFLAGS}

BIN_schemer = ${OBJDIR}/${NAME_schemer}
BIN_watcher = ${OBJDIR}/${NAME_watcher}
BIN_pcc = ${OBJDIR}/packcc

SRCS_COMMON = color_scheme.c datetime.c dbus.c log.c notify.c sigsource.c
SRCS_UNIQ_SCHEMER = ${OBJDIR}/${PEG}.c parser.c scheduler.c schemer.c settings.c
SRCS_UNIQ_WATCHER = watcher.c config.c escapes.c ini.c ivars.c x11.c
SRCS_schemer = ${SRCS_COMMON} ${SRCS_UNIQ_SCHEMER}
SRCS_watcher = ${SRCS_COMMON} ${SRCS_UNIQ_WATCHER}
SRCS_pcc = ${TOOLDIR}/packcc.c

SRCS_ALL = ${SRCS_COMMON} ${SRCS_UNIQ_SCHEMER} ${SRCS_UNIQ_WATCHER} \
	${SRCS_pcc}
TEST_SRCS_ALL =
TEST_NAMES =
TEST_BINS =
TEST_TRSS =

CMD_VERSION = cd ${TOP} && ${TOOLDIR}/get-version.sh --dirty ${VERSION_FILE}
CMD_DIST_VERSION = cd ${TOP} && ${TOOLDIR}/get-version.sh --clean ${VERSION_FILE}
# to reduce dependency spam when debugging makefiles, append:
#   sed -E 's/(^| )-I */\1-isystem /g'
CMD_GLIB_CFLAGS = pkg-config --cflags glib-2.0 gio-2.0
CMD_GLIB_LDFLAGS = pkg-config --libs glib-2.0 gio-2.0
CMD_XCB_CFLAGS = pkg-config --cflags xcb xcb-icccm xcb-xrm
CMD_XCB_LDFLAGS = pkg-config --libs xcb xcb-icccm xcb-xrm
