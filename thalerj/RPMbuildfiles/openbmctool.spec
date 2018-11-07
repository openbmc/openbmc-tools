# Copyright (c) 2017 International Business Machines.  All right reserved.
%define _binaries_in_noarch_packages_terminate_build   0
Summary: IBM OpenBMC tool
Name: openbmctool
Version: %{_version}
Release: %{_release}
License: BSD
Group: System Environment/Base
BuildArch: noarch
URL: http://www.ibm.com/
Source0: %{name}-%{version}-%{release}.tgz
Prefix: /opt
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

Requires: python34
Requires: python34-requests

#%if 0%{?_unitdir:1}
#Requires(post): systemd-units
#Requires(preun): systemd-units
#Requires(postun): systemd-units
#%endif

# Turn off the brp-python-bytecompile script
%global __os_install_post %(echo '%{__os_install_post}' | sed -e 's!/usr/lib[^[:space:]]*/brp-python-bytecompile[[:space:]].*$!!g')

%description
This package is to be applied to any linux machine that will be used to manage or interact with the IBM OpenBMC.
It provides key functionality to easily work with the IBM OpenBMC RESTful API, making BMC management easy.

#%build
#%{__make}
%prep
%setup -q -n %{name}-%{version}-%{release}

%install
#rm -rf $RPM_BUILD_ROOT
export DESTDIR=$RPM_BUILD_ROOT/opt/ibm/ras
mkdir -p $DESTDIR/bin
#mkdir -p $DESTDIR/bin/ppc64le
#mkdir -p $DESTDIR/etc
mkdir -p $DESTDIR/lib
#mkdir -p $RPM_BUILD_ROOT/usr/lib/systemd/system
cp openbmctool*.py $DESTDIR/bin
cp *.json $DESTDIR/lib


%clean
rm -rf $RPM_BUILD_ROOT

%files
#%defattr(-,root,root,-)
%attr(775,root,root) /opt/ibm/ras/bin/openbmctool.py
%attr(664,root,root)/opt/ibm/ras/lib/policyTable.json

%post
ln -s -f /opt/ibm/ras/bin/openbmctool.py /usr/bin/openbmctool
%changelog
