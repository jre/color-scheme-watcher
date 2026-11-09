${RECURSE_TARGETS}: ${OBJDIR}/Makefile ${OBJDIR}/GNUmakefile

${OBJDIR}/Makefile ${OBJDIR}/GNUmakefile:
	mkdir -p ${OBJDIR}
	cp ${TOP}/mk/templates/* ${OBJDIR}/

${OBJDIR}/Makefile: ${TOP}/mk/templates/Makefile
${OBJDIR}/GNUmakefile: ${TOP}/mk/templates/GNUmakefile

clean:
	rm -fr ${OBJDIR}

.PHONY: ${PHONY_TARGETS}

RECURSE_VARS = TOP=${TOP} OBJDIR=${OBJDIR}
