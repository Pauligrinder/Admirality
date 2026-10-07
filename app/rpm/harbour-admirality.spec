Name:       harbour-admirality
Summary:    Admirality — Wilma / Inschool.fi for Sailfish OS
Version:    0.2.10
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
    su -s /bin/sh defaultuser -c 'export XDG_RUNTIME_DIR=/run/user/100000; export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/100000/dbus/user_bus_socket; /usr/bin/systemctl --user daemon-reload; /usr/bin/systemctl --user enable harbour-admirality.service; /usr/bin/systemctl --user restart harbour-admirality.service' || true
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
* Wed Oct 07 2026 Pauli Kettunen <pauli.kettunen@sarkain.fi> - 0.2.10-1
- Show the current user in page headers; Change user in the pulley menu.
* Wed Oct 07 2026 Pauli Kettunen <pauli.kettunen@sarkain.fi> - 0.2.9-1
- Multi-person switcher on the unread card; auto-select the person with most unread on poll.
- Week-only schedule card (no day tap); subject boxes with meal icon for Ruokailu.
* Wed Oct 07 2026 Pauli Kettunen <pauli.kettunen@sarkain.fi> - 0.2.8-1
- Fix schedule Events widget collapsing after lessons load (flat list + height floor).
- Cover-blue schedule card with calendar watermark; show schedule above unread.
* Wed Oct 07 2026 Pauli Kettunen <pauli.kettunen@sarkain.fi> - 0.2.7-1
- Stop wiping the stored Wilma password when GetState omits it.
- Cover-blue info card with logo left/above the count buttons; simpler schedule widget.
* Wed Oct 07 2026 Pauli Kettunen <pauli.kettunen@sarkain.fi> - 0.2.6-1
- Flat schedule widget; white Wilma watermark in the blue bar; LauncherItem cold start.
- Quote daemon QT_MESSAGE_PATTERN so login survives service restarts.
* Wed Oct 07 2026 Pauli Kettunen <pauli.kettunen@sarkain.fi> - 0.2.5-1
- Size the schedule widget like the stock calendar loader; fix cold-start launch.
- Wilma logo and brand blue on the unread card; Admirality settings titles.
* Wed Oct 07 2026 Pauli Kettunen <pauli.kettunen@sarkain.fi> - 0.2.4-1
- Flatten the schedule Events widget so lipstick can size it like Helmsman.
- Run the Wilma daemon with a headless Qt platform; stop exposing the password on D-Bus.
* Wed Oct 07 2026 Pauli Kettunen <pauli.kettunen@sarkain.fi> - 0.2.3-1
- Load the schedule widget through a weather-style height-keeping Loader.
- Show press feedback on Wilma info icons and raise the app from lipstick.
* Wed Oct 07 2026 Pauli Kettunen <pauli.kettunen@sarkain.fi> - 0.2.2-1
- Launch the UI from the daemon when an Events view icon is tapped.
- Restart the Wilma service on upgrade so new D-Bus methods are live.
- Simplify the schedule widget so lipstick can measure and show it.
* Wed Oct 07 2026 Pauli Kettunen <pauli.kettunen@sarkain.fi> - 0.2.1-1
- Keep the schedule Events view widget from collapsing to zero height.
- Add a Wilma heading on the info card and open the matching page on tap.
* Wed Oct 07 2026 Pauli Kettunen <pauli.kettunen@sarkain.fi> - 0.2.0-1
- Load Wilma in a background service the app and Events View widgets share.
- School timetable and a counts card on the Events view.
- Mark a message read as soon as it is opened.
- Colour lesson notes red, green, or grey by type.
* Wed Aug 26 2026 Pauli Kettunen <pauli.kettunen@sarkain.fi> - 0.1.0-1
- Native Wilma picker, login, MFA, and message notifications.
