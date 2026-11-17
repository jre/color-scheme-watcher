all:

.include "${OBJDIR}/auto.mk"
.include "${TOP}/mk/defs-bsd.mk"
.include "${TOP}/mk/build-common.mk"

VERSION != ${CMD_VERSION}
DIST_VERSION != ${CMD_DIST_VERSION}

GLIB_CFLAGS ?!= ${CMD_GLIB_CFLAGS}
GLIB_LDFLAGS ?!= ${CMD_GLIB_LDFLAGS}
XCB_CFLAGS ?!= ${CMD_XCB_CFLAGS}
XCB_LDFLAGS ?!= ${CMD_XCB_LDFLAGS}

.PATH: ${SRCDIR} ${TOOLDIR} ${TESTDIR}
.SUFFIXES:
.SUFFIXES: .c .o

.for p in ${PROGS}
${p}: ${BIN_${p}}
${BIN_${p}}: ${OBJS_${p}}
	${CC} ${OBJS_${p}} ${LDFLAGS} ${LDFLAGS_${p}} -o $@
.endfor

.for t in ${TESTS}
${TEST_NAME_${t}}: ${TEST_TRS_${t}}
${TEST_TRS_${t}}: ${TEST_BIN_${t}}
	@${TESTRUNNER} ${TESTRUNNER_FLAGS_${t}} -- ${TEST_BIN_${t}}
${TEST_BIN_${t}}: ${TEST_OBJS_${t}}
	${CC} ${TEST_OBJS_${t}} ${LDFLAGS_TESTS} ${LDFLAGS_TEST_${t}} -o $@
.endfor

${DEPFILE}: .FORCE-REBUILD-KLUDGE
.PHONY: .FORCE-REBUILD-KLUDGE
.FORCE-REBUILD-KLUDGE:

-include ${DEPFILE}
