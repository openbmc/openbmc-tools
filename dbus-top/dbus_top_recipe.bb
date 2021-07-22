HOMEPAGE = "http://github.com/openbmc/openbmc-tools"
PR = "r1"
PV = "1.0+git${SRCPV}"

LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://../LICENSE;md5=e3fc50a88d0a364313df4b21ef20c29e"

SRC_URI += "git://github.com/openbmc-tools"
SRCREV = "4a0e2e3c10327dac1c923d263929be9a20478b24"

S = "${WORKDIR}/git/"
inherit meson

SUMMARY = "DBus-Top"
DESCRIPTION = "DBUs-Top."
GOOGLE_MISC_PROJ = "dbus-top"

DEPENDS += "systemd"
DEPENDS += "sdbusplus"
DEPENDS += "phosphor-mapper"
DEPENDS += "ncurses"

inherit systemd
