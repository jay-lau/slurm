AUTOMAKE_OPTIONS = foreign
# copied from pidgin
#
perl_dir = perl
perlpath = /usr/bin/perl
perl_sources = \
	       $(perl_dir)/Makefile.PL.in \
	       $(perl_dir)/ppport.h \
	       $(perl_dir)/Slurmdb.pm \
	       $(perl_dir)/Slurmdb.xs \
	       $(perl_dir)/slurmdb-perl.h \
	       $(perl_dir)/cluster.c

$(perl_dir)/Makefile:	$(perl_dir)/Makefile.PL
	@if test "x${top_srcdir}" != "x${top_builddir}"; then \
		for f in ${perl_sources}; do \
			${LN_S} -f ${abs_srcdir}/$$f $$f; \
		done; \
	fi
	@cd $(perl_dir) && $(perlpath) Makefile.PL $(PERL_MM_PARAMS) prefix=${prefix} INSTALL_BASE= PERL_MM_OPT=

#
# Note on linking logic below
#
# AIX needs to use LD to link. It can not use gcc.
# Suse Linux compiles with gcc, but picks some other compiler to use for linking.
#   Since some CFLAGS may be incompatible with this other compiler, the build
#   may fail, as seen on BlueGene platforms.
# Other Linux implementations sems to work fine with the LD specified as below
#
all-local: $(perl_dir)/Makefile #libslurmdb
if HAVE_AIX
	@cd $(perl_dir) && \
	if [ ! -f Makefile ]; then \
		$(perlpath) Makefile.PL $(PERL_MM_PARAMS) prefix=${prefix} INSTALL_BASE= PERL_MM_OPT=; \
	fi && \
	($(MAKE) CC="$(CC)" CCFLAGS="$(PERL_CFLAGS) -g -static $(CFLAGS) $(CPPFLAGS)" $(PERL_EXTRA_OPTS) || \
	 $(MAKE) CC="$(CC)" CCFLAGS="$(PERL_CFLAGS) -g -static $(CFLAGS) $(CPPFLAGS)" $(PERL_EXTRA_OPTS)) && \
	cd ..;
else
	@cd $(perl_dir) && \
	if [ ! -f Makefile ]; then \
		$(perlpath) Makefile.PL $(PERL_MM_PARAMS) prefix=${prefix} INSTALL_BASE= PERL_MM_OPT=; \
	fi && \
	($(MAKE) CC="$(CC)" LD="$(CC) $(CFLAGS) $(LDFLAGS)" CCFLAGS="$(PERL_CFLAGS) -g -static $(CFLAGS) $(CPPFLAGS)" $(PERL_EXTRA_OPTS) || \
	 $(MAKE) CC="$(CC)" LD="$(CC) $(CFLAGS) $(LDFLAGS)" CCFLAGS="$(PERL_CFLAGS) -g -static $(CFLAGS) $(CPPFLAGS)" $(PERL_EXTRA_OPTS)) && \
	cd ..;
endif

install-exec-local:
	@cd $(perl_dir) && \
	$(MAKE) DESTDIR=$(DESTDIR) install && \
	cd ..;

# Evil Hack (TM)
# ... which doesn't work with DESTDIR installs. FIXME?
uninstall-local:
	@cd $(perl_dir) && \
	`$(MAKE) uninstall | grep unlink | sed -e 's#/usr#${prefix}#' -e 's#unlink#rm -f#'` && \
	cd ..;

clean-generic:
	@cd $(perl_dir); \
	$(MAKE) clean; \
	cd ..; \
	rm -f *.so

distclean-generic:
	@cd $(perl_dir); \
	$(MAKE) realclean; \
	rm -f Makefile.PL; \
	rm -f Makefile.old; \
	rm -f Makefile; \
	cd ..;

	@rm -f Makefile

	@if test "x${top_srcdir}" != "x${top_builddir}"; then \
		for f in ${perl_sources}; do \
			${LN_S} -f ${abs_srcdir}/$$f $$f; \
		done; \
	fi

AM_CPPFLAGS = \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(DEBUG_CFLAGS) \
	$(PERL_CFLAGS)
