# GNU Makefile for ng-jackspa
# Copyright © 2012,2013 Géraud Meyer <graud@gmx.com>
#
# This file is part of ng-jackspa.
#
# ng-jackspa is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License version 2 as published by the
# Free Software Foundation.
#
# This package is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along
# with ng-jackspa.  If not, see <http://www.gnu.org/licenses/>.

prefix	?= $(HOME)/.local
bindir	?= $(prefix)/bin
datarootdir	?= $(prefix)/share
sysconfdir	?= $(prefix)/etc
docdir	?= $(datarootdir)/doc/$(PACKAGE_TARNAME)
mandir	?= $(datarootdir)/man
# DESTDIR =  # distributors set this on the command line

PACKAGE_NAME ?= ng-jackspa
PACKAGE_TARNAME	?= ngjackspa

RM ?= rm
CP ?= cp
LN ?= ln
MKDIR ?= mkdir
INSTALL ?= install
SED ?= sed
TAR ?= tar
TAR_FLAGS = --owner root --group root --mode a+rX,o-w --mtime .
MAKE ?= make
QMAKE = qmake
ASCIIDOC ?= asciidoc
ASCIIDOC_FLAGS = -apackagename="$(PACKAGE_NAME)" -aversion="$(VERSION)"
XMLTO ?= xmlto
MD5SUM ?= md5sum
SHA512SUM ?= sha512sum

# Get the version via git or from the VERSION file or from the project
# directory name.
VERSION	= $(shell test -x version.sh && ./version.sh $(PACKAGE_TARNAME) \
	  || echo "unknown_version")
# Allow either to be overwritten by setting DIST_VERSION on the command line.
ifdef DIST_VERSION
VERSION	= $(DIST_VERSION)
endif
PACKAGE_VERSION	= $(VERSION)
# Remove the _g<SHA1> part from the $VERSION
RPM_VERSION	= $(shell echo $(VERSION) | $(SED) -e 's/_g[0-9a-z]\+//')

CFLAGS ?= -Wall -Werror -g
CXXFLAGS ?= $(CFLAGS)
override CFLAGS += -DG_DISABLE_DEPRECATED `pkg-config --cflags glib-2.0`
override CXXFLAGS += -DG_DISABLE_DEPRECATED `pkg-config --cflags glib-2.0`
override LDFLAGS += -lm -ljack -ldl `pkg-config --libs glib-2.0` -g

PROGS = njackspa gjackspa qjackspa jackspa-cli
SCRIPTS =
MODULES = jackspa.o control.o
OBJECTS = $(MODULES) $(PROGS:%=%.o)
COMMONS = interface.c
TESTS =
QMAKE_PRO = qjackspa.pro
QMAKE_MAKEFILE = Makefile.qmake
MANDOC = ng-jackspa.1
MANLINKS = njackspa.1 gjackspa.1 qjackspa.1 jackspa-cli.1
MANTARGET = ng-jackspa.1
HTMLDOC = README.html INSTALL.html NEWS.html ng-jackspa.1.html
RELEASEDOC = $(MANDOC) $(HTMLDOC)

TARNAME	= $(PACKAGE_TARNAME)-$(RPM_VERSION)

all : build
.PHONY : all .help help build doc clean distclean dist install install-doc \
	force
.help :
	@echo "Available targets for $(PACKAGE_NAME) Makefile:"
	@echo "	.help all build clean doc distclean ChangeLog dist"
	@echo "	install install-doc"
	@echo "Useful variables for $(PACKAGE_NAME) Makefile:"
	@echo "	CFLAGS CXXFLAGS CPPFLAGS LDFLAGS prefix DESTDIR MAKE"
help : .help

build : $(PROGS) $(TESTS)
doc : $(MANDOC) $(HTMLDOC)

clean :
	-$(MAKE) -f $(QMAKE_MAKEFILE) clean mocclean
	$(RM) $(PROGS) $(OBJECTS) $(QMAKE_MAKEFILE)
distclean : clean
	$(RM) $(MANDOC) $(HTMLDOC)
	$(RM) -r $(TARNAME)
	$(RM) $(PACKAGE_TARNAME)-*.tar $(PACKAGE_TARNAME)-*.tar.gz \
	  $(PACKAGE_TARNAME)-*.tar.gz.md5 $(PACKAGE_TARNAME)-*.tar.gz.sha512 \
	  $(PACKAGE_TARNAME)-*.tar.gz.sig $(PACKAGE_TARNAME)-*.tar.gz.asc
maintainer-clean: distclean
	test ! -d .git && test ! -f .git || { $(RM) VERSION; $(RM) ChangeLog; }

install : build
	$(MKDIR) -p $(DESTDIR)$(bindir)
	set -e; for prog in $(PROGS) $(SCRIPTS); do \
		$(INSTALL) -p -m 0755 "$$prog" "$(DESTDIR)$(bindir)/"; \
	done
install-doc : doc
	$(MKDIR) -p $(DESTDIR)$(mandir)/man1
	set -e; for doc in $(MANDOC); do \
		gzip -9 <"$$doc" >"$$doc".gz; \
		$(INSTALL) -p -m 0644 "$$doc".gz "$(DESTDIR)$(mandir)/man1/"; \
		$(RM) "$$doc".gz; \
	done
	set -e; for doc in $(MANLINKS); do \
		$(LN) -s -f "$(MANTARGET)".gz "$(DESTDIR)$(mandir)/man1/$$doc".gz; \
	done
	$(MKDIR) -p $(DESTDIR)$(docdir)
	set -e; for doc in $(HTMLDOC); do \
		$(INSTALL) -p -m 0644 "$$doc" "$(DESTDIR)$(docdir)/"; \
	done

$(TARNAME).tar : ChangeLog
	$(MKDIR) -p $(TARNAME)
	echo $(VERSION) > $(TARNAME)/VERSION
	$(CP) -p ChangeLog $(TARNAME)
	git archive --format=tar --prefix=$(TARNAME)/ HEAD > $(TARNAME).tar
	$(TAR) $(TAR_FLAGS) -rf $(TARNAME).tar $(TARNAME)
	$(RM) -r $(TARNAME)
dist : $(TARNAME).tar.gz
nodocdist :
	$(MAKE) $(TARNAME).tar.gz NODOCDIST=yes
$(TARNAME).tar.gz :
	set -e; \
	if test -f ../$(TARNAME).tar.gz; then \
		echo NOTE: re-using already existing ../$(TARNAME).tar.gz; \
		$(CP) ../$(TARNAME).tar.gz .; \
	else \
		if test -d .git || test -f .git; then \
			$(MAKE) $(TARNAME).tar; \
		else \
			echo WARNING: re-building the distribution archive in ../$(TARNAME).tar; \
			sleep 3; \
			$(MAKE) distclean; \
			( cd .. && $(TAR) $(TAR_FLAGS) --mtime $(TARNAME) -cvf $(TARNAME).tar $(TARNAME) \
			  && $(MV) $(TARNAME).tar $(TARNAME)/ ); \
		fi; \
		if test x$(NODOCDIST) = x; then \
			$(MAKE) $(RELEASEDOC); \
			$(MKDIR) $(TARNAME); \
			$(CP) -p -P $(RELEASEDOC) $(TARNAME); \
			$(TAR) $(TAR_FLAGS) -rf $(TARNAME).tar $(TARNAME); \
			$(RM) -r $(TARNAME); \
		fi; \
		gzip -f -9 $(TARNAME).tar; \
	fi
	$(MD5SUM) $(TARNAME).tar.gz > $(TARNAME).tar.gz.md5
	$(SHA512SUM) $(TARNAME).tar.gz > $(TARNAME).tar.gz.sha512

ChangeLog :
	( echo "# $@ for $(PACKAGE_NAME) - automatically generated from the VCS's history"; \
	  echo; \
	  ./gitchangelog.sh --tags --tag-pattern 'version\/[^\n]*' \
	    -- - --date-order ) \
	| $(SED) 's/^\[version/\[version/' \
	> $@

njackspa : njackspa.c curses.c $(COMMONS) $(MODULES)
	$(CC) $(CFLAGS) -o $@ $< $(MODULES) $(LDFLAGS) -lncurses
gjackspa : gjackspa.cpp $(COMMONS) $(MODULES)
	$(CXX) $(CXXFLAGS) `pkg-config gtkmm-2.4 --cflags` -DGTK_DISABLE_DEPRECATED \
	  -o $@ $< $(MODULES) $(LDFLAGS) `pkg-config gtkmm-2.4 --libs`
qjackspa : force $(QMAKE_MAKEFILE)
	$(MAKE) -f $(QMAKE_MAKEFILE)
jackspa-cli : jackspa-cli.c $(COMMONS) $(MODULES)
	$(CC) -D_XOPEN_SOURCE -D_XOPEN_SOURCE_EXTENDED $(CFLAGS) \
	  -o $@ $< $(MODULES) $(LDFLAGS)

jackspa.o : ladspa.c

$(QMAKE_MAKEFILE) : $(QMAKE_PRO)
	$(QMAKE) -o $@

README.html : README BUGS asciidoc.conf
INSTALL.html : INSTALL asciidoc.conf
README.html INSTALL.html :
	$(ASCIIDOC) $(ASCIIDOC_FLAGS) -b xhtml11 -d article -a readme $<
NEWS.html : NEWS asciidoc.conf
	$(ASCIIDOC) $(ASCIIDOC_FLAGS) -b xhtml11 -d article $<
%.html : %.txt asciidoc.conf
	$(ASCIIDOC) $(ASCIIDOC_FLAGS) -b xhtml11 -d manpage $<
%.xml : %.txt asciidoc.conf
	$(ASCIIDOC) $(ASCIIDOC_FLAGS) -b docbook -d manpage $<
% : %.xml
	$(XMLTO) man $<

force :
