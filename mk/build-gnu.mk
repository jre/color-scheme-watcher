all:

include ${TOP}/mk/defs-gnu.mk
include ${TOP}/mk/build-common.mk

VPATH = ${SRCDIR} ${TOOLDIR} ${TESTDIR}

define mkprog_rules =
${1}: $${BIN_${1}}
$${BIN_${1}}: $${OBJS_${1}}
	$${CC} $${LDFLAGS_${1}} -o $$@ $${OBJS_${1}}
endef

define mktest_rules =
$${TEST_NAME_${1}}: $${TEST_TRS_${1}}
$${TEST_TRS_${1}}: $${TEST_BIN_${1}}
	@$${TESTRUNNER} $${TESTRUNNER_FLAGS_${1}} -- $${TEST_BIN_${1}}
$${TEST_BIN_${1}}: $${TEST_OBJS_${1}}
	$${CC} $${LDFLAGS_TESTS} $${LDFLAGS_TEST_${1}} -o $$@ $${TEST_OBJS_${1}}
endef

$(foreach p,${PROGS},$(eval $(call mkprog_rules,${p})))
$(foreach t,${TESTS},$(eval $(call mktest_rules,${t})))

-include ${DEPFILE}
