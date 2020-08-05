all: hwmon_bmk

# Edit the following two lines for use with BMC
# Note: because hwmon_bmk_bmc depends on libboost_system, you might need to copy
# its runtime library to the BMC as well.
S=$(HOME)/oecore-x86_64/environment-setup-armv7a-openbmc-linux-gnueabi
BMC_SYS_ROOT="$(HOME)/oecore-x86_64/sysroots/armv7a-openbmc-linux-gnueabi"

BMC_INCS=-I$(BMC_SYS_ROOT)/usr/include/
BMC_LIBS=--sysroot=$(BMC_SYS_ROOT)

hwmon_bmk: hwmon_bmk.cpp
	g++ hwmon_bmk.cpp -std=c++17 -lpthread -lboost_system -o $@


hwmon_bmk_bmc: hwmon_bmk.cpp
	@source $(S) && \
	 arm-openbmc-linux-gnueabi-g++ -std=c++17 $(BMC_INCS) $(BMC_LIBS) $^ -o $@ -lpthread -lboost_system -O2

clean:
	@if [ -f hwmon_bmk ]; then rm -v hwmon_bmk; fi
	@if [ -f hwmon_bmk_bmc ]; then rm -v hwmon_bmk_bmc; fi
