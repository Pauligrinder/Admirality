Name:       harbour-admirality
Summary:    Admirality — Wilma / Inschool.fi for Sailfish OS
Version:    0.1.0
Release:    1
License:    ASL 2.0
URL:        https://github.com/Pauligrinder/Admirality
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
Requires:   qt5-qtcore
Requires:   qt5-qtdeclarative
Requires:   sailfish-components-webview-qt5
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(qt5embedwidget)
BuildRequires:  desktop-file-utils

%description
Sailfish wrapper for Wilma / Inschool.fi. Enter your school's Wilma
address and use the service in a Silica WebView.

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake5_install

desktop-file-install --delete-original \
  --dir %{buildroot}%{_datadir}/applications \
  %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png

%changelog
* Wed Aug 26 2026 Pauli Kettunen <pauli.kettunen@sarkain.fi> - 0.1.0-1
- Initial packaging: WebView wrapper for Wilma / Inschool.fi.
