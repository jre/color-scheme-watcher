all:

include ${TOP}/mk/defs-gnu.mk
include ${TOP}/mk/wrapper-common.mk

MAKE_CMD = ${MAKE} ${MAKEFLAGS}

define mkrecurse
${1}:
	cd $${OBJDIR} && $${MAKE_CMD} $${RECURSE_VARS} ${1}
endef

$(foreach t,${RECURSE_TARGETS},$(eval $(call mkrecurse,${t})))
