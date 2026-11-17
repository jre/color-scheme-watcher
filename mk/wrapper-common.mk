${RECURSE_TARGETS}: ${OBJDIR}/Makefile ${OBJDIR}/GNUmakefile

${OBJDIR}/Makefile ${OBJDIR}/GNUmakefile:
	mkdir -p ${OBJDIR}
	cp ${TOP}/mk/templates/* ${OBJDIR}/

${OBJDIR}/Makefile: ${TOP}/mk/templates/Makefile
${OBJDIR}/GNUmakefile: ${TOP}/mk/templates/GNUmakefile

${OBJDIR}/config.status: ${TOOLDIR}/configure
	mkdir -p ${OBJDIR}
	cd ${OBJDIR} && ${TOOLDIR}/configure --srcdir=${TOP} ${CONFIG_FLAGS}

${OBJDIR}/auto.mk: ${TOP}/mk/auto.mk.in ${OBJDIR}/config.status
	cd ${OBJDIR} && ./config.status

${OBJDIR}/autoconfig.h: ${SRCDIR}/autoconfig.h.in ${OBJDIR}/config.status
	cd ${OBJDIR} && ./config.status && touch $@

clean:
	rm -fr ${OBJDIR}

install-home: install-schemer-home install-watcher-home

install-schemer-home: schemer
	cd ${OBJDIR} && ${MAKE_CMD} ${INSTHOME_VARS} install-schemer

install-watcher-home: watcher
	cd ${OBJDIR} && ${MAKE_CMD} ${INSTHOME_VARS} install-watcher

.PHONY: ${PHONY_TARGETS} install-home install-schemer-home install-watcher-home

RECURSE_DEPS = ${OBJDIR}/auto.mk ${OBJDIR}/autoconfig.h
RECURSE_VARS = TOP=${TOP} OBJDIR=${OBJDIR}
INSTHOME_VARS = ${RECURSE_VARS} INSTALL_FLAGS= \
	PREFIX=${HOME}/.local ETCDIR=${HOME}/.config \
	SHAREDIR=$${XDG_DATA_HOME:-${HOME}/.local/share}
