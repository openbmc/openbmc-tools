
/*****************************-*- ********-*-********************************
 * Filename:    libpwmtach.h                                                
 * Description: Library interface to pwmtach access                         
 *
 * Simple interface library for fan control operations
 * This file provides interface functions to support pwmtachtool.
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
 *
 *****************************************************************************/
#ifndef LIBPWMTACH_H
#define LIBPWMTACH_H

#ifdef __cplusplus
extern "C" {
#endif

#define PWMTACH_DEV_FILE   "/dev/pwmtach"

	/** \file libpwmtach.h
	 *  \brief Public headers for the PWMTACH interface library
	 *  
	 *  This library contains friendly function call interfaces for setting 
	 *  fan control operations.
	 */
	extern int set_fan_speed ( unsigned int dev_id, unsigned char fan_number, unsigned int rpm_value );
	extern int get_fan_speed ( unsigned int dev_id, unsigned char fan_number, unsigned int *rpm_value );
	/************/


	/******Pwmtach interface library to set/read pwm/tach by channel basis apart from configure fan table********/
	/*********Directly control pwm dutycycle (1 to 99) instead of RPM***********/
	extern int get_tach_speed ( unsigned int dev_id, unsigned char tach_number, unsigned int *rpm_value );
	//Notice: dutycycle_percentage value should be between 1 to 99.
	extern int set_pwm_dutycycle ( unsigned int dev_id, unsigned char pwm_number, unsigned char dutycycle_percentage );
	//Notice: dutycycle_value should be between 0 to 255.
	extern int set_pwm_dutycycle_value ( unsigned int dev_id, unsigned char pwm_number, unsigned char dutycycle_value );
	extern int get_pwm_dutycycle ( unsigned int dev_id, unsigned char pwm_number, unsigned char *dutycycle_percentage );

	/************/
#ifdef __cplusplus
}
#endif

#endif
