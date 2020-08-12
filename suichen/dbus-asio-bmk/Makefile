all: dbus_asio_bmk

# Edit the following two lines for use with BMC
# Note: because hwmon_bmk_bmc depends on libboost_system, you might need to copy
# its runtime library to the BMC as well.
S=$(HOME)/oecore-x86_64/environment-setup-armv7a-openbmc-linux-gnueabi
BMC_SYS_ROOT="$(HOME)/oecore-x86_64/sysroots/armv7a-openbmc-linux-gnueabi"

BMC_INCS=-I$(BMC_SYS_ROOT)/usr/include/
BMC_LIBS=--sysroot=$(BMC_SYS_ROOT)

dbus_asio_bmk: dbus_asio_bmk.cpp
	g++ $^ -std=c++17 -lpthread -lboost_system -lsystemd -lsdbusplus -O2 -o $@

dbus_asio_bmk_handlertracking: dbus_asio_bmk.cpp 
	g++ $^ -std=c++17 -lpthread -lboost_system -lsystemd -lsdbusplus -DBOOST_ASIO_ENABLE_HANDLER_TRACKING -O2 -o $@

dbus_asio_bmk_bmc: dbus_asio_bmk.cpp
	@source $(S) && \
	 arm-openbmc-linux-gnueabi-g++ -std=c++17 $(BMC_INCS) $(BMC_LIBS) $^ -o $@ -lpthread -lboost_system -lsystemd -lsdbusplus -O2

clean:
	@if [ -f dbus_asio_bmk ];     then rm -v dbus_asio_bmk; fi
	@if [ -f dbus_asio_bmk_bmc ]; then rm -v dbus_asio_bmk_bmc; fi
	@if [ -f dbus_asio_bmk_handlertracking ]; then rm -v dbus_asio_bmk_handlertracking; fi
