# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
# Use the pinned source epoch for RPM headers and installed file timestamps.
%global source_date_epoch_from_changelog 1
%global use_source_date_epoch_as_buildtime 1
%if v"%{rpmversion}" >= v"4.20"
%global build_mtime_policy clamp_to_source_date_epoch
%else
%global clamp_mtime_to_source_date_epoch 1
%endif
%bcond_without tests
%bcond_without docs
Name: qore-ssh2-module
Version: 2.0.0
Release: 3%{?dist}
Summary: SSH, SFTP, polling and file providers for Qore
License: LGPL-2.1-or-later OR MIT
URL: https://github.com/qoretechnologies/module-ssh2
Source0: %{name}-%{version}.tar.xz
%global _find_debuginfo_dwz_opts %{nil}
BuildRequires: cmake >= 3.5
BuildRequires: make
BuildRequires: gcc-c++
BuildRequires: pkgconfig(libssh2) >= 1.1
BuildRequires: pkgconfig(openssl)
BuildRequires: qore-devel >= 3.0.0~
BuildRequires: qore-rpm-macros >= 3.0.0~
%if %{with tests}
BuildRequires: python3
BuildRequires: openssh-server
BuildRequires: openssh-clients
BuildRequires: nss_wrapper
BuildRequires: qore-misc-tools >= 3.0.0~
%endif
%if %{with docs}
BuildRequires: doxygen
%if 0%{?suse_version}
BuildRequires: util-linux
%else
BuildRequires: util-linux-core
%endif
%endif
%{?qore_enable_aot_post}

%description
Native SSH2 and SFTP bindings, compiled and source file providers, polling
utilities and connection types. Includes compiler metadata and translations.
Uses the distribution's libssh2 and OpenSSL implementations.

%if %{with docs}
%package doc
Summary: SSH2 and SFTP module reference documentation
BuildArch: noarch
%description doc
Native and user-module API references for SSH, SFTP and file providers.
%endif

%prep
%autosetup
%build
%{?set_build_flags}
. %{_rpmconfigdir}/qore/module-env.sh
qore_set_source_prefix_maps "%{qore_debug_source_dir}"
cmake -S . -B build -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE=-DNDEBUG \
  -DCMAKE_INSTALL_PREFIX=%{_prefix} \
  -DCMAKE_SKIP_RPATH=ON -DCMAKE_IGNORE_PREFIX_PATH=/usr/local \
  -DQore_DIR=%{_libdir}/cmake/Qore -DQORE_EXECUTABLE=/usr/bin/qore \
  -DQORE_QPP_EXECUTABLE=/usr/bin/qpp -DQORE_QCC_EXECUTABLE=/usr/bin/qcc \
  -DQORE_BUILD_AOT_MODULES=ON -DQORE_AOT_LINK_SOURCE_MODULES=OFF \
  -DQORE_GENERATE_JAVA_BINDINGS=OFF -DQORE_SSH2_STRICT_DOCS=ON \
  -DQORE_QM_METADATA_ENV:STRING="QORE_MODULE_DIR=$QORE_MODULE_DIR:$PWD/qlib;QORE_MODULE_DIR_ONLY=1;QORE_INCLUDE_DIR=;LD_LIBRARY_PATH=" \
  -DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=%{!?with_docs:ON}%{?with_docs:OFF}
cmake --build build -- %{?_smp_mflags}
%if %{with docs}
cmake --build build --target docs -- %{?_smp_mflags}
%endif
%install
DESTDIR=%{buildroot} cmake --install build
%qore_install_aot_sources qlib
find %{buildroot}%{_libdir}/qore-modules -type f -name '*.qmod' -exec chmod 755 {} +
%if %{with docs}
install -d %{buildroot}%{_docdir}/%{name}-doc
cp -a build/docs %{buildroot}%{_docdir}/%{name}-doc/
hardlink -t -O %{buildroot}%{_docdir}/%{name}-doc
%endif
%check
%if %{with tests}
. %{_rpmconfigdir}/qore/module-env.sh
python3 -B -W error debian/tests/test_aot_metadata.py
python3 -B -W error rpm/test_fixture.py -v
python3 -B -W error rpm/run-tests.py --build-dir "$PWD/build"
qore-data-provider-i18n --no-color --check-source-tree --require-standard-locales \
  --require-complete-locales --output "$PWD/qlib"
%endif
%files
%license COPYING.MIT COPYING.LGPL
%doc README RELEASE-NOTES
%{_libdir}/qore-modules/*
%{_datadir}/qore-modules/*
%dir %{_datadir}/qore/metadata/ssh2
%{_datadir}/qore/metadata/ssh2/*.meta.json
%{_datadir}/qore/i18n/
%if %{with docs}
%files doc
%license COPYING.MIT COPYING.LGPL
%doc %{_docdir}/%{name}-doc/
%endif
%changelog
* Tue Oct 06 2026 David Nichols <david@qore.org> - 2.0.0-3
- Use the installed Qore SDK module directories without an unused CMake option.

* Thu Oct 01 2026 David Nichols <david@qore.org> - 2.0.0-2
- Package native and compiled modules, metadata, locales and API references.
- Run all eight suites against a private server with ephemeral keys and home.
