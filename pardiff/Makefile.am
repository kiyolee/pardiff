####### kdevelop will overwrite this part!!! (begin)##########
bin_PROGRAMS = pardiff
pardiff_SOURCES = pardiff.c  parcdiff.c pardiff.h
pardiff_LDADD   = 


EXTRA_DIST = pardiff.c diffp pardiff.1 diffp.1 

install-data-local:
	$(mkinstalldirs) ${mandir}/man1/
	$(INSTALL_DATA) pardiff.1 ${mandir}/man1/pardiff.1
	$(mkinstalldirs) ${mandir}/man1/
	$(INSTALL_DATA) diffp.1 ${mandir}/man1/diffp.1

install-exec-local:
	$(mkinstalldirs) ${bindir}/
	$(INSTALL_SCRIPT) diffp ${bindir}/diffp

uninstall-local:
	-rm -f ${bindir}/diffp
	-rm -f ${mandir}/man1/pardiff.1
	-rm -f ${mandir}/man1/diffp.1

####### kdevelop will overwrite this part!!! (end)############
bin_PROGRAMS = pardiff
