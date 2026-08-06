MNAME=ssh2

if COND_MINGW
QPP=qpp_native
else
QPP=qpp
endif

.qpp.cpp:
	$(QPP) -V $<

GENERATED_SRC = QC_SSH2Base.cpp QC_SSH2Client.cpp QC_SSH2Channel.cpp QC_SFTPClient.cpp QC_SftpPollOperation.cpp
CLEANFILES = $(GENERATED_SRC)

if COND_SINGLE_COMPILATION_UNIT
single-compilation-unit.cpp: $(GENERATED_SRC)
SSH2_SOURCES = single-compilation-unit.cpp
else
SSH2_SOURCES = ssh2-module.cpp SSH2Client.cpp SFTPClient.cpp SftpPollOperation.cpp SSH2Channel.cpp
nodist_ssh2_la_SOURCES = $(GENERATED_SRC)
endif

lib_LTLIBRARIES = ssh2.la
ssh2_la_SOURCES = $(SSH2_SOURCES)
ssh2_la_LDFLAGS = -module -avoid-version $(QORE_LDFLAGS) $(LIBSSH2_LDFLAGS)

AM_CPPFLAGS = $(LIBSSH2_CPPFLAGS) $(QORE_CPPFLAGS) $(OPENSSL_CPPFLAGS)

if COND_PROFILE
AM_CXXFLAGS = -g -pg
else
if COND_DEBUG
AM_CXXFLAGS = -g
endif
endif

if COND_MINGW
RM_OTHER=$(DESTDIR)$(libdir)/$(MNAME).dll.a
endif

install-exec-hook:
	mv $(DESTDIR)$(libdir)/$(MNAME).$(MODULE_SUFFIX) $(DESTDIR)$(libdir)/$(MNAME)-api-$(MODULE_API).qmod
	$(RM) $(DESTDIR)$(libdir)/$(MNAME).la $(DESTDIR)$(libdir)/$(MNAME).a $(RM_OTHER)
