#!/usr/bin/make -f
###### GNU make #########
# 
# Byggning av handböcker
#
#########################

OS = $(shell uname -s)

ifeq ("$(OS)", "Linux")
DBK = /usr/share/xml/docbook/stylesheet/nwalsh
endif

KODNING = ISO-8859-1

NAME = ndav

VERSION = 0.0.1

DBKSOURCE = handbok.xml

HANDBOK = handbok-$(OS).xml

MANPAGES = man/$(NAME).1

.PHONY: version manpages clean rensa

manpages: version $(MANPAGES)

version:
	perl -i -p -e 's#__version__#$(VERSION)#' $(DBKSOURCE)

clean rensa:
	rm -rf $(HANDBOK) man/ xhtml/

$(HANDBOK): $(DBKSOURCE)
	xsltproc --novalid \
		--output "$@" \
		--stringparam	profile.os "$(OS)" \
		$(DBK)/profiling/profile.xsl $<
	@LANG=C perl -i -p -e 's/^\s*\.\s*$$//' $@

man/%.1: $(HANDBOK)
	xsltproc --novalid \
		--param			man.output.quietly			1		\
		--param			man.output.in.separate.dir	1		\
		--param			man.output.subdirs.enabled	0		\
		--stringparam	man.output.base.dir			man/	\
		--stringparam	man.output.encoding		$(KODNING)	\
			$(DBK)/manpages/docbook.xsl $<
	@LANG=C perl -i -p -e 's#sv\.([1-8])#$$1#' man/*.sv.[1-8]
	xsltproc --novalid \
		--param			chunk.quietly		1		\
		--param			use.id.as.filename	1		\
		--stringparam	base.dir			xhtml/			\
		--stringparam	chunker.output.encoding	$(KODNING)	\
		--stringparam	html.stylesheet		$(NAME).css		\
			$(DBK)/xhtml/chunk.xsl $<
	@install -m 0644 ndav.css xhtml/$(NAME).css
	@(which xmllint > /dev/null; \
		if [ $$? -eq 0 ]; then \
			for namn in xhtml/*.html; do \
				xmllint --format --output $$namn.tlf $$namn; \
				mv $$namn.tlf $$namn; \
			done; \
		fi )

.PHONY: test test-xtml
test:
	xmllint --noout --valid $(DBKSOURCE)

test-xhtml:
	@if [ ! -d xhtml/ ]; then \
		echo "Inga xhtml-filer."; exit 1; \
	else \
		xmllint --noout --valid xhtml/*.html; \
	fi
