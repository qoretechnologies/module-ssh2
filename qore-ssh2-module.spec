%define mod_ver 2.0.0

%{?_datarootdir: %global mydatarootdir %_datarootdir}
%{!?_datarootdir: %global mydatarootdir /usr/share}

%define module_api %(qore --latest-module-api 2>/dev/null)
%define module_dir %{_libdir}/qore-modules
%global user_module_dir %{mydatarootdir}/qore-modules/

%if 0%{?sles_version}

%define dist .sles%{?sles_version}

%else
%if 0%{?suse_version}

# get *suse release major version
%define os_maj %(echo %suse_version|rev|cut -b3-|rev)
# get *suse release minor version without trailing zeros
%define os_min %(echo %suse_version|rev|cut -b-2|rev|sed s/0*$//)

%if %suse_version > 1010
%define dist .opensuse%{os_maj}_%{os_min}
%else
%define dist .suse%{os_maj}_%{os_min}
%endif

%endif
%endif

# see if we can determine the distribution type
%if 0%{!?dist:1}
%define rh_dist %(if [ -f /etc/redhat-release ];then cat /etc/redhat-release|sed "s/[^0-9.]*//"|cut -f1 -d.;fi)
%if 0%{?rh_dist}
%define dist .rhel%{rh_dist}
%else
%define dist .unknown
%endif
%endif

Summary: SSH2 module for Qore
Name: qore-ssh2-module
Version: %{mod_ver}
Release: 1%{dist}
%if 0%{?suse_version}
License: LGPL-2.0+ or GPL-2.0+ or MIT
%else
License: LGPLv2+ or GPLv2+ or MIT
%endif
Group: Development/Languages
URL: http://www.qoretechnologies.com/qore
Source: http://prdownloads.sourceforge.net/qore/%{name}-%{version}.tar.bz2
#Source0: %{name}-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: cmake >= 3.5
BuildRequires: gcc-c++
BuildRequires: qore-devel >= 3.0
BuildRequires: qore-stdlib >= 3.0
BuildRequires: qore >= 3.0
BuildRequires: libssh2-devel >= 1.1
BuildRequires: openssl-devel
BuildRequires: doxygen
%if 0%{?el7}
BuildRequires: devtoolset-7-gcc-c++
%endif
Requires: /usr/bin/env
Requires: qore-module(abi)%{?_isa} = %{module_api}

%description
SSH2 module for the Qore Programming Language.


%if 0%{?suse_version}
%debug_package
%endif

%prep
%setup -q

%build
%if 0%{?el7}
# enable devtoolset7
. /opt/rh/devtoolset-7/enable
%endif
export CXXFLAGS="%{?optflags}"
cmake -DCMAKE_INSTALL_PREFIX=%{_prefix} -DCMAKE_BUILD_TYPE=RELWITHDEBINFO -DCMAKE_SKIP_RPATH=1 -DCMAKE_SKIP_INSTALL_RPATH=1 -DCMAKE_SKIP_BUILD_RPATH=1 -DCMAKE_PREFIX_PATH=${_prefix}/lib64/cmake/Qore .
make %{?_smp_mflags}
make %{?_smp_mflags} docs

%install
make DESTDIR=%{buildroot} install %{?_smp_mflags}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{module_dir}
%{user_module_dir}
%doc COPYING.LGPL COPYING.MIT README RELEASE-NOTES AUTHORS

%package doc
Summary: SSH2 module for Qore
Group: Development/Languages

%description doc
SSH2 module for the Qore Programming Language.

This RPM provides API documentation, test and example programs


%files doc
%defattr(-,root,root,-)
%doc docs/ssh2/ docs/SftpClientDataProvider/ docs/SftpPoller/ docs/SftpPollerUtil/ docs/Ssh2Connections/ test/

%changelog
* Thu Aug 6 2026 David Nichols <david@qore.org> - 2.0.0
- updated to version 2.0.0

* Fri May 31 2024 David Nichols <david@qore.org> - 1.5.0
- updated to version 1.5.0

* Sat Aug 12 2023 David Nichols <david@qore.org> - 1.4.2
- updated to version 1.4.2

* Fri Jan 14 2022 David Nichols <david@qore.org> - 1.4.1
- updated to version 1.4.1

* Sun May 10 2020 David Nichols <david@qore.org> - 1.4
- updated to version 1.4

* Sun Jan 28 2018 David Nichols <david@qore.org> - 1.3
- updated to version 1.3

* Thu Feb 2 2017 David Nichols <david@qore.org> - 1.2
- updated to version 1.2

* Thu Feb 2 2017 Pavel Kveton <pavel.kveton@qoretechnologies.org> - 1.1
- updated to version 1.1

* Sat Dec 7 2013 David Nichols <david@qore.org> - 1.0
- updated to version 1.0

* Fri Aug 16 2013 David Nichols <david@qore.org> - 0.9.9
- updated to version 0.9.9

* Mon Aug 5 2013 David Nichols <david@qore.org> - 0.9.8.1
- updated to version 0.9.8.1

* Tue Mar 12 2013 David Nichols <david@qore.org> - 0.9.8
- updated to version 0.9.8

* Fri Jun 8 2012 David Nichols <david@qore.org> - 0.9.7
- updated for qpp build

* Mon Dec 19 2011 David Nichols <david@qore.org> - 0.9.7
- updated to version 0.9.7

* Mon Dec 20 2010 David Nichols <david@qore.org>
- updated to version 0.9.6

* Wed Jun 16 2010 David Nichols <david@qore.org>
- updated to version 0.9.5

* Mon Jan 11 2010 David Nichols <david_nichols@users.sourceforge.net>
- initial spec file for ssh2 module
