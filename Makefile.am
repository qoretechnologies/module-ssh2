moddir = $(userlibdir)

AUTOMAKE_OPTIONS = foreign

SUBDIRS = src

noinst_HEADERS = src/ssh2-module.h \
	src/ssh2.h \
	src/SSH2Client.h \
	src/SFTPClient.h \
	src/SSH2Channel.h \
	src/QC_SSH2Base.h

USER_MODULES = qlib/SftpClientDataProvider \
    qlib/SftpPollerUtil.qm \
    qlib/SftpPoller.qm \
	qlib/Ssh2Connections.qm

EXTRA_DIST = COPYING.LGPL COPYING.MIT AUTHORS README \
	RELEASE-NOTES \
	src/QC_SSH2Base.qpp \
	src/QC_SSH2Client.qpp \
	src/QC_SSH2Channel.qpp \
	src/QC_SFTPClient.qpp \
	test/Ssh2Client.qtest \
    test/Ssh2Connections.qtest \
	test/SFTPClient.qtest \
	test/SftpPollerMultiDirs.qtest \
	test/SftpPoller.qtest \
	$(USER_MODULES) \
	qore-ssh2-module.spec

ACLOCAL_AMFLAGS=-I m4

dist_mod_DATA = $(USER_MODULES)

if COND_DOXYGEN
DOX = ssh2 SftpClientDataProvider SftpPollerUtil SftpPoller Ssh2Connections
DOXYGEN_OUTPUT=docs/ssh2/html docs/SftpClientDataProvider/html docs/SftpPollerUtil/html docs/SftpPoller/html docs/Ssh2Connections/html
DOXYFILES = docs/doxyfile docs/mainpage.doxygen
DOXYFILES_SRC = docs/doxyfile.tmpl docs/footer_template.html
QLIB_TMP_DOXYFILES = docs/qlib/SftpClientDataProvider.qm.dox.h docs/qlib/SftpPollerUtil.qm.dox.h docs/qlib/SftpPoller.qm.dox.h docs/qlib/Ssh2Connections.qm.dox.h docs/qlib/Doxyfile.SftpPollerUtil docs/qlib/Doxyfile.SftpPoller docs/qlib/Doxyfile.Ssh2Connections docs/ssh2.tag docs/SftpPollerUtil.tag docs/SftpPoller.tag docs/Ssh2Connections.tag
QDX = qdx
DX_CLEANFILES = ${DOXYFILES} $(QLIB_TMP_DOXYFILES)

docs/qlib/Doxyfile.tmp: docs/qlib/Doxyfile.tmpl
	sed s:__USERMODDIR__:${usermoddir}: $< > $@

docs/qlib/Doxyfile.SftpClientDataProvider: docs/qlib/Doxyfile.tmp
	$(QDX) -M=qlib/SftpClientDataProvider:qlib/SftpClientDataProvider.qm.dox.h -tssh2.tag=../../ssh2/html $< $@

docs/qlib/SftpClientDataProvider.qm.dox.h: qlib/SftpClientDataProvider
	$(QDX) $< $@

docs/SftpClientDataProvider/html: docs/qlib/SftpClientDataProvider.qm.dox.h docs/qlib/Doxyfile.SftpClientDataProvider
	cd docs; $(DOXYGEN_CMD) qlib/Doxyfile.SftpClientDataProvider
	$(QDX) --post docs/SftpClientDataProvider/html docs/SftpClientDataProvider/html/search

docs/qlib/Doxyfile.SftpPollerUtil: docs/qlib/Doxyfile.tmp
	$(QDX) -M=qlib/SftpPollerUtil.qm:qlib/SftpPollerUtil.qm.dox.h -tssh2.tag=../../ssh2/html $< $@

docs/qlib/SftpPollerUtil.qm.dox.h: qlib/SftpPollerUtil.qm
	$(QDX) $< $@

docs/SftpPollerUtil/html: docs/qlib/SftpPollerUtil.qm.dox.h docs/qlib/Doxyfile.SftpPollerUtil
	cd docs; $(DOXYGEN_CMD) qlib/Doxyfile.SftpPollerUtil
	$(QDX) --post docs/SftpPollerUtil/html docs/SftpPollerUtil/html/search

docs/qlib/Doxyfile.SftpPoller: docs/qlib/Doxyfile.tmp
	$(QDX) -M=qlib/SftpPoller.qm:qlib/SftpPoller.qm.dox.h -tssh2.tag=../../ssh2/html -tSftpPollerUtil.tag=../../SftpPollerUtil/html $< $@

docs/qlib/SftpPoller.qm.dox.h: qlib/SftpPoller.qm
	$(QDX) $< $@

docs/SftpPoller/html: docs/qlib/SftpPoller.qm.dox.h docs/qlib/Doxyfile.SftpPoller
	cd docs; $(DOXYGEN_CMD) qlib/Doxyfile.SftpPoller
	$(QDX) --post docs/SftpPoller/html docs/SftpPoller/html/search

docs/qlib/Doxyfile.Ssh2Connections: docs/qlib/Doxyfile.tmp
	$(QDX) -M=qlib/Ssh2Connections.qm:qlib/Ssh2Connections.qm.dox.h -tssh2.tag=../../ssh2/html $< $@

docs/qlib/Ssh2Connections.qm.dox.h: qlib/Ssh2Connections.qm
	$(QDX) $< $@

docs/Ssh2Connections/html: docs/qlib/Ssh2Connections.qm.dox.h docs/qlib/Doxyfile.Ssh2Connections
	cd docs; $(DOXYGEN_CMD) qlib/Doxyfile.Ssh2Connections
	$(QDX) --post docs/Ssh2Connections/html docs/Ssh2Connections/html/search

docs/ssh2/html: $(DOXYFILES)
	cd docs; $(DOXYGEN_CMD) doxyfile;
	$(QDX) --post docs/ssh2/html docs/ssh2/html/search

docs/mainpage.doxygen: docs/mainpage.doxygen.tmpl
	$(QDX) -d $< $@

docs/doxyfile: docs/doxyfile.tmpl
	sed -e s/__VERSION__/${VERSION}/ -e s:__USERMODDIR__:${usermoddir}: $< > $@

doxygen-doc: $(DOXYGEN_OUTPUT)

EXTRA_DIST += $(DOXYGEN_OUTPUT)

html-local: $(DOXYGEN_OUTPUT)

install-html-local:
	for dox in $(DOX); do $(MKDIR_P) $(DESTDIR)$(datadir)/$(PACKAGE)/docs/$$dox/html/search; done
	for dir in $(DOXYGEN_OUTPUT); do \
	   for f in $$dir/*; do if [ -f $$f ]; then $(INSTALL_DATA) $$f $(DESTDIR)$(datadir)/$(PACKAGE)/$$dir; fi; done; \
	   for f in $$dir/search/*; do $(INSTALL_DATA) $$f $(DESTDIR)$(datadir)/$(PACKAGE)/$$dir/search; done; \
	done

clean-local:
	$(RM) -rf $(DOXYGEN_OUTPUT)

uninstall-local:
	$(RM) -rf $(DESTDIR)$(datadir)/$(PACKAGE)/docs

mostlyclean-local:
	rm -rf ${DOXYGEN_OUTPUT} ${DX_CLEANFILES}

else
doxygen-doc:
	echo no doxygen support
endif

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
