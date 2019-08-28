# FRUID-UTIL
Usage: fruid-util 

This utility displays all FRU properties that are available through the FRUDevice service on the D-Bus.

Example:
	root@tiogapass:/tmp# ./fruid-util.bin 
	FRU Information           : Tioga_Pass_Single_Side
	---------------           : ------------------
	ADDRESS                   : 84
	BOARD_INFO_AM1            : 02-000243
	BOARD_INFO_AM2            : LBG-1G
	BOARD_LANGUAGE_CODE       : 0
	BOARD_MANUFACTURER        : Wistro
	BOARD_MANUFACTURE_DATE    : Fri Aug 25 19:17:00 2017
	BOARD_PART_NUMBER         : B81.00X10.0109
	BOARD_PRODUCT_NAME        : Tioga Pass Single Side
	BOARD_SERIAL_NUMBER       : WTH1734038ZSA
	BUS                       : 6
	CHASSIS_INFO_AM1          : M7MX546700026
	CHASSIS_INFO_AM2          : _71C9N5700774
	CHASSIS_PART_NUMBER       : B81.00X01.0104
	CHASSIS_SERIAL_NUMBER     : WTF17370D2MA1
	CHASSIS_TYPE              : 23
	Common_Format_Version     : 1
	PRODUCT_ASSET_TAG         : 3718452
	PRODUCT_INFO_AM1          : 01-002572
	PRODUCT_INFO_AM2          : 1505546857
	PRODUCT_LANGUAGE_CODE     : 0
	PRODUCT_MANUFACTURER      : Wiwynn
	PRODUCT_PART_NUMBER       : B81.00X01.0104
	PRODUCT_PRODUCT_NAME      : Type6 Tioga Pass Single Side
	PRODUCT_SERIAL_NUMBER     : WTF17370D2MA1
	PRODUCT_VERSION           : PVT 

Note: All FRU information that is reported as a numeric value will be displayed in decimal. 