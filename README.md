# Admirality for Sailfish OS

A Silica Harbour wrapper (`harbour-admirality`) for [Wilma / Inschool.fi](https://inschool.fi).
Built with Qt 5 / QML + C++ the same way most SFOS apps are, and tested against
Platform SDK target `SailfishOS-5.2.0.15-aarch64`.

Enter your school's Wilma address (for example `espoo` or `https://espoo.inschool.fi`)
and the app opens it in a WebView. Session cookies stay in the app profile.
Choosing a school from inschool.fi stores that Wilma address for the next launch.

Cover actions reload the page and open settings.

## Layout

```
app/
  harbour-admirality.pro
  src/harbour-admirality.cpp
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
scp app/RPMS/harbour-admirality-0.1.0-1.aarch64.rpm defaultuser@<phone-ip>:~/
ssh defaultuser@<phone-ip>
devel-su pkcon install-local ~/harbour-admirality-0.1.0-1.aarch64.rpm
```

Sailjail permissions used: `Internet`.

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
