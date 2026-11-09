all:

.include "${TOP}/mk/defs-bsd.mk"
.include "${TOP}/mk/build-common.mk"

.PATH: ${SRCDIR} ${TOOLDIR} ${TESTDIR}
.SUFFIXES:
.SUFFIXES: .c .o

.for p in ${PROGS}
${p}: ${BIN_${p}}
${BIN_${p}}: ${OBJS_${p}}
	${CC} ${LDFLAGS} ${LDFLAGS_${p}} -o $@ ${OBJS_${p}}
.endfor

.for t in ${TESTS}
${TEST_NAME_${t}}: ${TEST_TRS_${t}}
${TEST_TRS_${t}}: ${TEST_BIN_${t}}
	@${TESTRUNNER} ${TESTRUNNER_FLAGS_${t}} -- ${TEST_BIN_${t}}
${TEST_BIN_${t}}: ${TEST_OBJS_${t}}
	${CC} ${LDFLAGS_TESTS} ${LDFLAGS_TEST_${t}} -o $@ ${TEST_OBJS_${t}}
.endfor

${DEPFILE}: .FORCE-REBUILD-KLUDGE
.PHONY: .FORCE-REBUILD-KLUDGE
.FORCE-REBUILD-KLUDGE:

-include ${DEPFILE}
