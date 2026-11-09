include ${TOP}/mk/defs-common.mk

VERSION := $(shell ${CMD_VERSION})
DIST_VERSION := $(shell ${CMD_DIST_VERSION})
GLIB_CFLAGS := $(shell ${CMD_GLIB_CFLAGS})
GLIB_LDFLAGS := $(shell ${CMD_GLIB_LDFLAGS})
XCB_CFLAGS := $(shell ${CMD_XCB_CFLAGS})
XCB_LDFLAGS := $(shell ${CMD_XCB_LDFLAGS})

define mkprog_defs =
OBJS_${1} = $$(notdir $${SRCS_${1}:.c=.o})
endef

define mktest_defs =
TEST_NAME_${1} = test-${1}
TEST_BIN_${1} = $${OBJDIR}/test_${1}}
TEST_SRCS_${1} = test_${1}.c $${TEST_DEPS_ALL} $${TEST_DEPS_${1}}
TEST_OBJS_${1} = $(notdir $${TEST_SRCS_${1}:.c=.o})
TEST_TRS_${1} = $${OBJDIR}/test-${1}.trs
TESTRUNNER_FLAGS_${1} = --test-name ${1} --log-file test-${1}.log --trs-file $${TEST_TRS_${1}}
TEST_SRCS_ALL += $${TESTDIR}/test_${1}.c
TEST_NAMES += $${TEST_NAME_${1}}
TEST_BINS += $${TEST_BIN_${1}}
TEST_TRSS += $${TEST_TRS_${1}}
endef

$(foreach p,${PROGS},$(eval $(call mkprog_defs,${p})))
$(foreach t,${TESTS},$(eval $(call mktest_defs,${t})))
