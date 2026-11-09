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

install: install-schemer install-watcher

install-schemer:
	rm -f ${OBJDIR}/${DBUS_SERVICE} \
		${OBJDIR}/${NAME_schemer}.service \
		${OBJDIR}/LICENSE-packcc.txt
	sed "s|%SCHEMER_PATH%|${LIBEXECDIR}/${NAME_schemer}|" \
		< ${TOP}/dbus/${DBUS_SERVICE} > ${OBJDIR}/${DBUS_SERVICE}
	sed "s|%SCHEMER_PATH%|${LIBEXECDIR}/${NAME_schemer}|" \
		< ${TOP}/systemd/${NAME_schemer}.service \
		> ${OBJDIR}/${NAME_schemer}.service
	sed -n '1,/\*\//s|^[/ *]*||p' \
		< ${SRCS_pcc} > ${OBJDIR}/LICENSE-packcc.txt
	${INSTALL_DIR} ${DESTDIR}${LIBEXECDIR} ${DESTDIR}${PORTALDIR} \
		${DESTDIR}${DBUSDIR} ${DESTDIR}${UNITDIR} \
		${DESTDIR}${MAN1DIR} ${DESTDIR}${MAN5DIR} \
		${DESTDIR}${SHAREDIR}/${NAME_schemer} \
		${DESTDIR}${SHAREDIR}/examples/${NAME_schemer}
	${INSTALL_BIN} ${BIN_schemer} ${DESTDIR}${LIBEXECDIR}
	${INSTALL_DATA} ${TOP}/xdp/schemer.portal ${DESTDIR}${PORTALDIR}
	${INSTALL_DATA} ${OBJDIR}/${DBUS_SERVICE} ${DESTDIR}${DBUSDIR}
	${INSTALL_DATA} ${OBJDIR}/${NAME_schemer}.service ${DESTDIR}${UNITDIR}
	${INSTALL_DATA} ${TOP}/doc/${NAME_schemer}.1.man \
		${DESTDIR}${MAN1DIR}/${NAME_schemer}.1
	${INSTALL_DATA} ${TOP}/doc/${NAME_schemer}.conf.5.man \
		${DESTDIR}${MAN5DIR}/${NAME_schemer}.conf.5
	${INSTALL_DATA} ${TOP}/LICENSE.txt ${OBJDIR}/LICENSE-packcc.txt \
		${DESTDIR}${SHAREDIR}/${NAME_schemer}
	${INSTALL_DATA} ${TOP}/examples/${NAME_schemer}*.conf \
		${TOP}/examples/portals*.conf \
		${DESTDIR}${SHAREDIR}/examples/${NAME_schemer}

install-watcher:
	rm -f ${OBJDIR}/${NAME_watcher}.service
	sed "s|%WATCHER_PATH%|${BINDIR}/${NAME_watcher}|" \
		< ${TOP}/systemd/${NAME_watcher}.service \
		> ${OBJDIR}/${NAME_watcher}.service
	${INSTALL_DIR} ${DESTDIR}${BINDIR} ${DESTDIR}${UNITDIR} \
		${DESTDIR}${MAN1DIR} ${DESTDIR}${MAN5DIR} \
		${DESTDIR}${SHAREDIR}/${NAME_watcher} \
		${DESTDIR}${SHAREDIR}/examples/${NAME_watcher}
	${INSTALL_BIN} ${BIN_watcher} ${DESTDIR}${BINDIR}
	${INSTALL_DATA} ${OBJDIR}/${NAME_watcher}.service ${DESTDIR}${UNITDIR}
	${INSTALL_DATA} ${TOP}/doc/${NAME_watcher}.1.man \
		${DESTDIR}${MAN1DIR}/${NAME_watcher}.1
	${INSTALL_DATA} ${TOP}/doc/${NAME_watcher}.ini.5.man \
		${DESTDIR}${MAN5DIR}/${NAME_watcher}.ini.5
	${INSTALL_DATA} ${TOP}/LICENSE.txt ${SRCDIR}/LICENSE-inih.txt \
		${DESTDIR}${SHAREDIR}/${NAME_watcher}
	${INSTALL_DATA} ${TOP}/examples/${NAME_watcher}*.ini \
		${TOP}/examples/xresources* ${TOP}/examples/*.sh \
		${DESTDIR}${SHAREDIR}/examples/${NAME_watcher}

${DEPFILE}: ${SRCS_ALL} ${TEST_SRCS_ALL}
	cd ${SRCDIR} && ${CC} ${CFLAGS} ${CFLAGS_depend} \
		-MM ${SRCS_ALL} ${TEST_SRCS_ALL} > ${OBJDIR}/$@

${OBJDIR}/${PEG}.c ${OBJDIR}/${PEG}.h: ${BIN_pcc} ${SRCDIR}/${PEG}.peg
	${BIN_pcc} ${PCC_FLAGS} -o ${OBJDIR}/${PEG} ${SRCDIR}/${PEG}.peg

.c.o:
	${CC} ${CFLAGS} ${CFLAGS_$*} -c -o $@ $<
