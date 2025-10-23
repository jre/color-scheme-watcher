MANDOC ?= mandoc
MANDOC_FLAGS = -mdoc
MARKDOWN_FIXUP = ./mdlinks.py ${PAGES}

PAGES = color-scheme-watcher.1 color-scheme-watcher.ini.5
MDOC_FILES = ${PAGES:=.man}
MARKDOWN_FILES = ${PAGES:=.md}

all: markdown

markdown: ${MARKDOWN_FILES}

lint:
	${MANDOC} -T lint ${MDOC_FILES}

clean:
	rm -f ${MARKDOWN_FILES}

.PHONY: all markdown lint clean

.SUFFIXES: .man .md

.man.md:
	${MANDOC} ${MANDOC_FLAGS} -T markdown $< | ${MARKDOWN_FIXUP} > $@
