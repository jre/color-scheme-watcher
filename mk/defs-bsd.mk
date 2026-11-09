.include "${TOP}/mk/defs-common.mk"

VERSION != ${CMD_VERSION}
DIST_VERSION != ${CMD_DIST_VERSION}
GLIB_CFLAGS != ${CMD_GLIB_CFLAGS}
GLIB_LDFLAGS != ${CMD_GLIB_LDFLAGS}
XCB_CFLAGS != ${CMD_XCB_CFLAGS}
XCB_LDFLAGS != ${CMD_XCB_LDFLAGS}

.for p in ${PROGS}
OBJS_${p} = ${SRCS_${p}:T:.c=.o}
.endfor

.for t in ${TESTS}
TEST_NAME_${t} = test-${t}
TEST_BIN_${t} = ${OBJDIR}/test_${t}
TEST_SRCS_${t} = test_${t}.c ${TEST_DEPS_ALL} ${TEST_DEPS_${t}}
TEST_OBJS_${t} = ${TEST_SRCS_${t}:T:.c=.o}
TEST_TRS_${t} = ${OBJDIR}/test-${t}.trs
TESTRUNNER_FLAGS_${t} = --test-name ${t} --log-file test-${t}.log --trs-file ${TEST_TRS_${t}}
TEST_SRCS_ALL += ${TESTDIR}/test_${t}.c
TEST_NAMES += ${TEST_NAME_${t}}
TEST_BINS += ${TEST_BIN_${t}}
TEST_TRSS += ${TEST_TRS_${t}}
.endfor
