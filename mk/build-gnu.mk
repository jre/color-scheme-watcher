all:

include ${OBJDIR}/auto.mk
include ${TOP}/mk/defs-gnu.mk
include ${TOP}/mk/build-common.mk

VERSION := $(shell ${CMD_VERSION})
DIST_VERSION := $(shell ${CMD_DIST_VERSION})

ifeq ($(origin GLIB_CFLAGS), undefined)
GLIB_CFLAGS := $(shell ${CMD_GLIB_CFLAGS})
endif
ifeq ($(origin GLIB_LDFLAGS), undefined)
GLIB_LDFLAGS := $(shell ${CMD_GLIB_LDFLAGS})
endif
ifeq ($(origin XCB_CFLAGS), undefined)
XCB_CFLAGS := $(shell ${CMD_XCB_CFLAGS})
endif
ifeq ($(origin XCB_LDFLAGS), undefined)
XCB_LDFLAGS := $(shell ${CMD_XCB_LDFLAGS})
endif

VPATH = ${SRCDIR} ${TOOLDIR} ${TESTDIR}

define mkprog_rules =
${1}: $${BIN_${1}}
$${BIN_${1}}: $${OBJS_${1}}
	$${CC} $${OBJS_${1}} $${LDFLAGS_${1}} -o $$@
endef

define mktest_rules =
$${TEST_NAME_${1}}: $${TEST_TRS_${1}}
$${TEST_TRS_${1}}: $${TEST_BIN_${1}}
	@$${TESTRUNNER} $${TESTRUNNER_FLAGS_${1}} -- $${TEST_BIN_${1}}
$${TEST_BIN_${1}}: $${TEST_OBJS_${1}}
	$${CC} $${TEST_OBJS_${1}} $${LDFLAGS_TESTS} $${LDFLAGS_TEST_${1}} -o $$@
endef

$(foreach p,${PROGS},$(eval $(call mkprog_rules,${p})))
$(foreach t,${TESTS},$(eval $(call mktest_rules,${t})))

-include ${DEPFILE}
