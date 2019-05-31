/*
* Gpiotool Application
* This application provides functions to get / set GPIO's Direction,
* Data by using the sysfs.
* Copyright (C) <2019>  <American Megatrends International LLC>
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef __GPIO__
#define __GPIO__

/* common  declaritions for gpio library and the driver */
#define GPIO_DIR_OD_OUT 2
#define GPIO_DIR_OUT   1
#define GPIO_DIR_IN    0
#define GPIO_POL_HIGH  1
#define GPIO_POL_LOW   0
#define GPIO_DATA_HIGH 1
#define GPIO_DATA_LOW  0
#define GPIO_ENABLE_PULL_DOWN       0
#define GPIO_ENABLE_PULL_UP         1
#define GPIO_DISABLE_PULL_UP_DOWN   2
#define GPIO_ENABLE_DEBOUNCE       1
#define GPIO_DISABLE_DEBOUNCE      0

typedef struct gpio_list_data_info
{
	unsigned short PinNum;
	unsigned char  data;
}  __attribute__((packed)) gpio_list_data_info;

typedef struct gpio_list_data
{
	unsigned int count;
	gpio_list_data_info *info;
}  __attribute__((packed)) gpio_list_data;

typedef struct
{
	unsigned char id;
}__attribute__((packed)) gpio_property_t;

/****This is the structure that is passed back and forth between userspace and driver as an ioctl arg*****/
typedef struct 
{
	unsigned short PinNum; /* Not used in case of interrupt sensor data */
	union
	{
		unsigned char  data; /* Direction or Value or Polarity */
		gpio_list_data gpio_list;

	};
	gpio_property_t property;
}  __attribute__((packed)) Gpio_data_t;	

#define GET_MODULE_GPIO_PROPERTIES      _IOC(_IOC_WRITE,'K',0x00,0x3FFF)
#define GET_GPIO_DIRECTION              _IOC(_IOC_WRITE,'K',0x01,0x3FFF)
#define SET_GPIO_DIRECTION              _IOC(_IOC_WRITE,'K',0x02,0x3FFF)
#define GET_GPIO_POLARITY               _IOC(_IOC_WRITE,'K',0x03,0x3FFF)
#define SET_GPIO_POLARITY               _IOC(_IOC_WRITE,'K',0x04,0x3FFF)
#define GET_GPIO_PULL_UP_DOWN           _IOC(_IOC_WRITE,'K',0x05,0x3FFF)
#define SET_GPIO_PULL_UP_DOWN           _IOC(_IOC_WRITE,'K',0x06,0x3FFF)
#define READ_GPIO                       _IOC(_IOC_WRITE,'K',0x07,0x3FFF)
#define WRITE_GPIO                      _IOC(_IOC_WRITE,'K',0x08,0x3FFF)
#define SET_GPIO_OD_OUT                 _IOC(_IOC_WRITE,'K',0x09,0x3FFF)
#define SET_CONT_MODE                   _IOC(_IOC_WRITE,'K',0x12,0x3FFF)
#define READ_GPIOS                      _IOC(_IOC_WRITE,'K',0x13,0x3FFF)
#define WRITE_GPIOS                     _IOC(_IOC_WRITE,'K',0x14,0x3FFF)
#define GET_GPIOS_DIR                   _IOC(_IOC_WRITE,'K',0x15,0x3FFF)
#define SET_GPIOS_DIR                   _IOC(_IOC_WRITE,'K',0x16,0x3FFF)
#define SET_GPIO_PROPERTY               _IOC(_IOC_WRITE,'K',0x18,0x3FFF)
#define GET_GPIO_PROPERTY               _IOC(_IOC_WRITE,'K',0x19,0x3FFF)
#define GET_GPIO_DEBOUNCE               _IOC(_IOC_WRITE,'K',0x1A,0x3FFF)
#define SET_GPIO_DEBOUNCE               _IOC(_IOC_WRITE,'K',0x1B,0x3FFF)
#define GET_GPIO_DEBOUNCE_CLOCK         _IOC(_IOC_WRITE,'K',0x1C,0x3FFF)
#define SET_GPIO_DEBOUNCE_CLOCK         _IOC(_IOC_WRITE,'K',0x1D,0x3FFF)
#define PULSE_GPIO                      _IOC(_IOC_WRITE,'K',0x20,0x3FFF)  // option for PULSE_GPIO ioctl command

#define MAX_NUMBER_OF_TOGGLE_INSTANCE 4
typedef struct 
{
	unsigned short int ToggleCycle; //< multiple with 10ms(1 kernel tick)
	struct
	{
		unsigned int CyclePeriod	: 20;	//< bit[0:19] mark for end of period
		unsigned int Reserved 		: 12;	//< bit[20:31]  	

	}Conf;
	struct
	{
		unsigned long long int  timeout;	//< timeout in 10ms. LED will switch off, 0xFF means no time out 
		unsigned int Pattern 		: 20; 	//< bit[0:19];
		unsigned int defaultOff		: 1;	//< bit[20)
		unsigned int enableTimeOut  : 1;	//< bit[21)
		unsigned int Reserved		: 1;	//< bit[22]	
		unsigned int Valid			: 1;	//< bit[23]
		unsigned int Port			: 5;	//< bit[24:28] Port Number
		unsigned int Number			: 3;	//< bit[29:31] GPIO Number	
	}Gpio[MAX_NUMBER_OF_TOGGLE_INSTANCE];
}__attribute__ ((packed)) ToggleData;


typedef Gpio_data_t gpio_ioctl_data;

#endif // __GPIO__
