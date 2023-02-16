HOMEPAGE = "http://github.com/openbmc/openbmc-tools"
PR = "r1"
PV = "1.0+git${SRCPV}"

LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://../LICENSE;md5=e3fc50a88d0a364313df4b21ef20c29e"

SRC_URI += "git://github.com/openbmc/openbmc-tools"
SRCREV = "ffb4d52e0c9ca6dadbc55feb4f94795be1be1732"

S = "${WORKDIR}/git/dbus-top/"
inherit meson

SUMMARY = "DBus-Top"
DESCRIPTION = "DBUs-Top."
GOOGLE_MISC_PROJ = "dbus-top"

DEPENDS += "systemd"
DEPENDS += "sdbusplus"
DEPENDS += "phosphor-mapper"
DEPENDS += "ncurses"

inherit systemd
