/*****************************-*- ********-*-********************************/
/* Filename:    gpioifc.h                                                    */
/* Description: Library interface to gpio access                             */
/*****************************************************************************/
/** \file gpioifc.h
 *  \brief Public headers for the gpio interface library
 *  
 *  This library contains friendly function call interfaces for setting 
 *  gpio directions and data.  It hides all the details of playing with
 *  gpios through the gpio manager (opening the device file, calling ioctl,
 *  etc.)
 */

#ifndef GPIOIFC_H
#define GPIOIFC_H

#ifdef __cplusplus
extern "C" {
#endif

	extern int get_gpio_dir ( unsigned short gpio_number );
	extern int set_gpio_dir_input ( unsigned short gpio_number );
	extern int set_gpio_dir_output ( unsigned short gpio_number );
	extern int get_gpio_pol ( unsigned short gpio_number );
	extern int set_gpio_pol_high ( unsigned short gpio_number );
	extern int set_gpio_pol_low ( unsigned short gpio_number );
	extern int get_gpio_data ( unsigned short gpio_number );
	extern int get_gpios_data ( int count, unsigned short *gpio_number, unsigned char *result);
	extern int set_gpio_data_high  ( unsigned short gpio_number );
	extern int set_gpio_data_low ( unsigned short gpio_number ); 
	extern int set_gpios_data  ( int count, unsigned short *gpio_number, int data );
	extern int get_gpios_dir  ( int count, unsigned short *gpio_number, unsigned char *result );
	extern int set_gpios_dir  ( int count, unsigned short *gpio_number, int dir );
	extern int set_gpio_od_output_high( unsigned short gpio_number );
	extern int set_gpio_od_output_low( unsigned short gpio_number );
	extern int set_gpio_pull_up( unsigned short gpio_number );
	extern int set_gpio_pull_down( unsigned short gpio_number );
	extern int set_gpio_no_pull_up_down( unsigned short gpio_number );
	extern int get_gpio_pull_up_down ( unsigned short gpio_number );
	extern int set_sgpio_cont_mode_high  ( unsigned short gpio_number );
	extern int set_gpio_property( unsigned short gpionum, unsigned char property_id, unsigned char property_value);
	extern int get_gpio_property( unsigned short gpionum, unsigned char property_id, unsigned char * property_value);
	extern int get_gpio_debounce ( unsigned short gpio_number );
	extern int set_gpio_debounce ( unsigned short gpio_number, unsigned char gpio_data );
	extern int enable_gpio_debounce( unsigned short gpio_number );
	extern int disable_gpio_debounce( unsigned short gpio_number );
	extern int get_gpio_debounce_clock ( unsigned short gpio_number );
	extern int set_gpio_debounce_clock ( unsigned short gpio_number, unsigned char gpio_data );
	extern int pulse_gpio_data_high  ( unsigned short gpio_number );
	extern int pulse_gpio_data_low ( unsigned short gpio_number );
#ifdef __cplusplus
}
#endif

#endif
