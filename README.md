# Admirality for Sailfish OS

A Silica Harbour wrapper (`harbour-admirality`) for [Wilma / Inschool.fi](https://inschool.fi).
Built with Qt 5 / QML + C++ the same way most SFOS apps are, and tested against
Platform SDK target `SailfishOS-5.2.0.15-aarch64`.

The city/Wilma picker, login, home, messages, news, schedule, and notifications
are native. The Wilma website stays available from the pulley menu and settings
as a fallback. Login and the school list follow the same
unofficial Wilma flow used by [wilmai](https://github.com/aikarjal/wilmai)
and [OpenWilma](https://github.com/OpenWilma/openwilma.js):

1. **Choose Wilma** — searchable list of Finnish Wilma tenants (bundled from
   wilmai's public directory), or a custom address.
2. **Sign in** — username and password against that tenant. TOTP is supported
   when Wilma asks for it.
3. **Notifications** — a user service (`harbour-admiralityd`) keeps the
   Wilma session and polls on the interval chosen in settings: every 15
   minutes, once an hour, every 3 hours, or 15 minutes before and after
   the school day. New messages, notes, announcements, grades, homework,
   and exams raise a Sailfish notification.

The app and the Events view widgets both read that service over the
session bus. Schedule shows the current day, with arrows and a week
timetable; after the school week has ended the week view opens on next
week. The info card shows icon counts only. Opening a message marks it
read immediately. Lesson notes use a green, red, or grey dot.

Credentials stay in the app's local settings so the session can be restored.

Cover actions refresh native Wilma data and open settings.

After install, enable the widgets under Settings → Events view and restart
the home screen if they do not appear.

## Layout

```
app/
  harbour-admirality.pro
  daemon/harbour-admiralityd.pro
  harbour-admirality.service
  eventsview/
  src/wilmaclient.{h,cpp}
  src/wilmaservice.{h,cpp}
  src/wilmabridge.{h,cpp}
  data/tenant_list.json
  qml/pages/
  rpm/harbour-admirality.spec
```

## Build

Docker Platform SDK flow:

```sh
docker pull coderus/sailfishos-platform-sdk-aarch64
chmod +x build.sh
./build.sh
```

Install on the phone:

```sh
scp app/RPMS/harbour-admirality-0.2.0-1.aarch64.rpm defaultuser@<phone-ip>:~/
ssh defaultuser@<phone-ip>
devel-su pkcon install-local ~/harbour-admirality-0.2.0-1.aarch64.rpm
```

Sailjail permissions used: `Internet`, `Notifications`, `AdmiralityDBus`.

## Releases (GitHub Actions)

CI builds Sailfish RPMs with the Platform SDK Docker image (`5.2.0.15`)
for `aarch64`, `armv7hl`, and `i486`.

**Automatic release:** bump `VERSION` in `app/harbour-admirality.pro`,
merge to `main`. If no GitHub release exists for that version yet, the
Release workflow builds the RPMs and publishes a GitHub Release. Rebuild
tags count too, so a `v0.1.0-2` release stops `main` from republishing
`v0.1.0` as build 1.

**Manual tag:** after bumping `VERSION`, you can also tag explicitly:

```sh
git tag v0.1.0
git push origin v0.1.0
```

Optional RPM release counter in the tag: `v0.1.0-2`.

Pull requests and pushes to `main` also run a CI build (aarch64 only)
without publishing a release.
