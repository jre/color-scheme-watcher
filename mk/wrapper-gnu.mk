all:

include ${TOP}/mk/defs-gnu.mk
include ${TOP}/mk/wrapper-common.mk

define mkrecurse
${1}:
	cd ${OBJDIR} && ${MAKE} ${MAKEFLAGS} ${RECURSE_VARS} ${1}
endef

$(foreach t,${RECURSE_TARGETS},$(eval $(call mkrecurse,${t})))
