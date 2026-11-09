all:

.include "${TOP}/mk/defs-bsd.mk"
.include "${TOP}/mk/wrapper-common.mk"

.for t in ${RECURSE_TARGETS}
${t}:
	cd ${OBJDIR} && ${.MAKE} ${.MAKEFLAGS} ${RECURSE_VARS} ${t}
.endfor
