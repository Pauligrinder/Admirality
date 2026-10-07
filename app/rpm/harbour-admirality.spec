Name:       harbour-admirality
Summary:    Admirality — Wilma / Inschool.fi for Sailfish OS
Version:    0.2.0
Release:    1
License:    ASL 2.0
URL:        https://github.com/Pauligrinder/Admirality
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
Requires:   qt5-qtcore
Requires:   qt5-qtdeclarative
Requires:   qt5-qtnetwork
Requires:   qt5-qtdbus
Requires:   sailfish-components-webview-qt5
Requires:   nemo-qml-plugin-notifications-qt5
Requires:   nemo-qml-plugin-dbus-qt5
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(qt5embedwidget)
BuildRequires:  desktop-file-utils

%description
Sailfish wrapper for Wilma / Inschool.fi. A background service loads Wilma
and the app and Events View widgets both read that. Native city/school
picker, login, timetable, and notifications when new messages, notes,
announcements, or grades show up.

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
make %{?_smp_mflags}
mkdir -p daemon-build
(cd daemon-build && qmake ../daemon/harbour-admiralityd.pro && make %{?_smp_mflags})

%install
rm -rf %{buildroot}
%qmake5_install
make -C daemon-build install INSTALL_ROOT=%{buildroot}

desktop-file-install --delete-original \
  --dir %{buildroot}%{_datadir}/applications \
  %{buildroot}%{_datadir}/applications/*.desktop

mkdir -p %{buildroot}/usr/lib/systemd/user/user-session.target.wants
ln -sf ../%{name}.service %{buildroot}/usr/lib/systemd/user/user-session.target.wants/%{name}.service

%post
if [ -S /run/user/100000/dbus/user_bus_socket ]; then
    su -s /bin/sh defaultuser -c 'export XDG_RUNTIME_DIR=/run/user/100000; export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/100000/dbus/user_bus_socket; /usr/bin/systemctl --user daemon-reload; /usr/bin/systemctl --user enable --now harbour-admirality.service' || true
fi

%preun
if [ "$1" = "0" ] && [ -S /run/user/100000/dbus/user_bus_socket ]; then
    su -s /bin/sh defaultuser -c 'export XDG_RUNTIME_DIR=/run/user/100000; export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/100000/dbus/user_bus_socket; /usr/bin/systemctl --user disable --now harbour-admirality.service' || true
fi

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_bindir}/%{name}d
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%{_datadir}/lipstick/eventswidgets/%{name}.json
%{_datadir}/translations/%{name}_eng_en.qm
/usr/lib/systemd/user/%{name}.service
/usr/lib/systemd/user/user-session.target.wants/%{name}.service
%config %{_sysconfdir}/sailjail/permissions/AdmiralityDBus.permission

%changelog
* Wed Oct 07 2026 Pauli Kettunen <pauli.kettunen@sarkain.fi> - 0.2.0-1
- Load Wilma in a background service the app and Events View widgets share.
- School timetable and a counts card on the Events view.
- Mark a message read as soon as it is opened.
- Colour lesson notes red, green, or grey by type.
* Wed Aug 26 2026 Pauli Kettunen <pauli.kettunen@sarkain.fi> - 0.1.0-1
- Native Wilma picker, login, MFA, and message notifications.
