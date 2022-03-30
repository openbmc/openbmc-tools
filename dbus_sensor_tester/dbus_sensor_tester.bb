LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = ""

SRC_URI = "git://git@github.com:openbmc/openbmc-tools.git;protocol=ssh"

PV = "1.0+git${SRCPV}"
SRCREV = "e2f90b8cbbed95d8aef762350d1d49c157c6fa79"

S = "$WORKDIR}/git/dbus_sensor_tester"

inherit meson pkgconfig

DEPENDS += " \
    sdbusplus \
    boost \
    cli11 \
"
