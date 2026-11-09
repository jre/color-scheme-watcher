all:

.include "${TOP}/mk/defs-bsd.mk"
.include "${TOP}/mk/wrapper-common.mk"

MAKE_CMD = ${.MAKE} ${.MAKEFLAGS}

.for t in ${RECURSE_TARGETS}
${t}:
	cd ${OBJDIR} && ${MAKE_CMD} ${RECURSE_VARS} ${t}
.endfor
