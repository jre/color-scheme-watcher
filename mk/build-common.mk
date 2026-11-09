.PHONY: ${PHONY_TARGETS}

all: ${PROGS}

clean:
	rm -fr ${OBJDIR}

depend: ${DEPFILE}

build-tests: ${TEST_BINS}

check test tests regress: ${TOOLDIR}/trs-aggregate.sh ${TEST_TRSS}
	@${TOOLDIR}/trs-aggregate.sh -v "${VERSION}" -o ${TEST_LOG} ${TEST_TRSS}

dist distribute:
	rm -rf ${DIST_NAME}.${DIST_EXT}
	cd ${TOP} && git archive -o ${OBJDIR}/${DIST_NAME}.${DIST_EXT} \
		--format=${DIST_EXT} --prefix=${DIST_NAME}/ \
		--add-virtual-file=${DIST_NAME}/${VERSION_FILE}:${DIST_VERSION} \
		HEAD

${DEPFILE}: ${SRCS_ALL} ${TEST_SRCS_ALL}
	cd ${SRCDIR} && ${CC} ${CFLAGS} ${CFLAGS_depend} \
		-MM ${SRCS_ALL} ${TEST_SRCS_ALL} > ${OBJDIR}/$@

${OBJDIR}/${PEG}.c ${OBJDIR}/${PEG}.h: ${BIN_pcc} ${SRCDIR}/${PEG}.peg
	${BIN_pcc} ${PCC_FLAGS} -o ${OBJDIR}/${PEG} ${SRCDIR}/${PEG}.peg

.c.o:
	${CC} ${CFLAGS} ${CFLAGS_$*} -c -o $@ $<
