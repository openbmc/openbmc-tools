/*
 * gpioifc.c
 *
 * Simple interface for setting gpio directions, polarity, and data
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <glob.h>
#include "gpio.h"
#include "gpioifc.h"
#define SYSFS_GPIO_DIR   "/sys/class/gpio"
#define MAX_BUF 40
#define VAL_BUF 5

static int gpio_export(unsigned int gpio)
{
	int fd, len;
	char commandStrBuf[MAX_BUF];
	memset(commandStrBuf,0,MAX_BUF);
	snprintf(commandStrBuf, sizeof(commandStrBuf), SYSFS_GPIO_DIR "/gpio%d", gpio);
	if (access(commandStrBuf, F_OK) != 0)
	{
		//TINFO("Folder %s is not exist, export it\n", commandStrBuf);
		fd = sigwrap_open(SYSFS_GPIO_DIR "/export", O_WRONLY);
		if (fd < 0) {
			printf("%d gpio/export failed\n",gpio);
			return fd;
		}
		memset(commandStrBuf,0,MAX_BUF);
		len = snprintf(commandStrBuf, sizeof(commandStrBuf), "%d", gpio);
		write(fd, commandStrBuf, len);
		sigwrap_close(fd);
	}
	return 0;
}

static int gpio_get_dir(unsigned int gpio, unsigned char *value)
{
	int fd;
	char ch[VAL_BUF];
	char commandStrBuf[MAX_BUF];
	memset(commandStrBuf,0,MAX_BUF);
	snprintf(commandStrBuf, sizeof(commandStrBuf), SYSFS_GPIO_DIR "/gpio%d/direction", gpio);
	fd = sigwrap_open(commandStrBuf, O_RDONLY);
	if (fd < 0) {
		printf("Get %d gpio/direction\n",gpio);
		return fd;
	}
	read(fd, ch, VAL_BUF);
	if (strncmp(ch,"out",3) == 0) {
		*value = 1;
	} else {
		*value = 0;
	}
	sigwrap_close(fd);
	return 0;
}

static int gpio_set_dir(unsigned int gpio, unsigned char out_flag)
{
	int fd;
	char commandStrBuf[MAX_BUF];
	memset(commandStrBuf,0,MAX_BUF);
	snprintf(commandStrBuf, sizeof(commandStrBuf), SYSFS_GPIO_DIR "/gpio%d/direction", gpio);
	fd = sigwrap_open(commandStrBuf, O_WRONLY);
	if (fd < 0) {
		printf("set %d gpio/direction 0x%x\n",gpio,out_flag);
		return fd;
	}
	if (out_flag)
		write(fd, "out", 4);
	else
		write(fd, "in", 3);
	sigwrap_close(fd);
	return 0;
}

static int gpio_get_value(unsigned int gpio, unsigned char *value)
{
	int fd;
	char ch;
	char commandStrBuf[MAX_BUF];
	memset(commandStrBuf,0,MAX_BUF);
	snprintf(commandStrBuf, sizeof(commandStrBuf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
	fd = sigwrap_open(commandStrBuf, O_RDONLY);
	if (fd < 0) {
		printf("Get %d gpio/value\n",gpio);
		return fd;
	}
	read(fd, &ch, 1);
	if (ch != '0') {
		*value = 1;
	} else {
		*value = 0;
	}
	sigwrap_close(fd);
	return 0;
}

static int gpio_set_value(unsigned int gpio, unsigned char value)
{
	int fd;
	char commandStrBuf[MAX_BUF];
	memset(commandStrBuf,0,MAX_BUF);
	snprintf(commandStrBuf, sizeof(commandStrBuf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
	fd = sigwrap_open(commandStrBuf, O_WRONLY);
	if (fd < 0) {
		printf("set %d gpio/value 0x%x\n",gpio,value);
		return fd;
	}
	if (value)
		write(fd, "1", 2);
	else
		write(fd, "0", 2);
	sigwrap_close(fd);
	return 0;
}

static int gpio_get_polarity(unsigned int gpio, unsigned char *value)
{
	int fd;
	char ch;
	char commandStrBuf[MAX_BUF];
	memset(commandStrBuf,0,MAX_BUF);
	snprintf(commandStrBuf, sizeof(commandStrBuf), SYSFS_GPIO_DIR "/gpio%d/active_low", gpio);
	fd = sigwrap_open(commandStrBuf, O_RDONLY);
	if (fd < 0) {
		printf("Get %d gpio/active_low\n",gpio);
		return fd;
	}
	read(fd, &ch, 1);
	if (ch != '0') {
		*value = 1;
	} else {
		*value = 0;
	}
	sigwrap_close(fd);
	return 0;
}

static int gpio_set_polarity(unsigned int gpio, unsigned char value)
{
	int fd;
	char commandStrBuf[MAX_BUF];
	memset(commandStrBuf,0,MAX_BUF);
	snprintf(commandStrBuf, sizeof(commandStrBuf), SYSFS_GPIO_DIR "/gpio%d/active_low", gpio);
	fd = sigwrap_open(commandStrBuf, O_WRONLY);
	if (fd < 0) {
		printf("set %d gpio/active_low 0x%x\n",gpio,value);
		return fd;
	}
	if (value)
		write(fd, "1", 2);
	else
		write(fd, "0", 2);
	sigwrap_close(fd);
	return 0;
}

#define SET_GPEVEN                  0x02
/** \file gpioifc.c
  \brief Source for all gpio interface code
 */

/**
 * Gpio_action
 * 
 **/
static int gpio_action(  gpio_ioctl_data *argp, int command )
{
	int retval = -1;
	int pinNum;
	unsigned int i;

	/* do action according to commands */
	switch(command)
	{
		case GET_GPIO_DIRECTION:
			pinNum = argp->PinNum; gpio_export(pinNum);
			if(gpio_get_dir(pinNum,(unsigned char *)&argp->data) == 0)
				retval = (int)argp->data;
			break;
		case SET_GPIO_DIRECTION:
			pinNum = argp->PinNum; gpio_export(pinNum);
			if(gpio_set_dir(pinNum, (unsigned char)argp->data) == 0)
				retval = (int)argp->data;
			break;
		case READ_GPIO:
			pinNum = argp->PinNum; gpio_export(pinNum);
			if(gpio_get_value(pinNum,(unsigned char *)&argp->data) == 0)
				retval = (int)argp->data;
			break;
		case WRITE_GPIO:
			pinNum = argp->PinNum; gpio_export(pinNum);
			if(gpio_set_value(pinNum, (unsigned char)argp->data) == 0)
				retval = (int)argp->data;
			break;
		case GET_GPIO_POLARITY:
			pinNum = argp->PinNum; gpio_export(pinNum);
			if(gpio_get_polarity(pinNum,(unsigned char *)&argp->data) == 0)
				retval = (int)argp->data;
			break;
		case SET_GPIO_POLARITY:
			pinNum = argp->PinNum; gpio_export(pinNum);
			if(gpio_set_polarity(pinNum, (unsigned char)argp->data) == 0)
				retval = (int)argp->data;
			break;
		case GET_GPIOS_DIR:
			for(i=0;i<argp->gpio_list.count;i++)
			{
				pinNum = (int)(argp->gpio_list.info)[i].PinNum;
				gpio_export(pinNum);
				if(gpio_get_dir(pinNum,(unsigned char *)&(argp->gpio_list.info)[i].data) == 0)
					retval = 0;
				else {retval = -1;break;}
			}
			break;
		case READ_GPIOS:
			for(i=0;i<argp->gpio_list.count;i++)
			{
				pinNum = (int)(argp->gpio_list.info)[i].PinNum;
				gpio_export(pinNum);
				if(gpio_get_value(pinNum,(unsigned char *)&(argp->gpio_list.info)[i].data) == 0)
					retval = 0;
				else {retval = -1;break;}
			}
			break;
		case WRITE_GPIOS:
			for(i=0;i<argp->gpio_list.count;i++)
			{
				pinNum = (int)(argp->gpio_list.info)[i].PinNum;
				gpio_export(pinNum);
				if(gpio_set_value(pinNum,(argp->gpio_list.info)[i].data) == 0)
					retval = 0;
				else {retval = -1;break;}

			}
			break;
		case SET_GPIOS_DIR:
			for(i=0;i<argp->gpio_list.count;i++)
			{
				pinNum = (int)(argp->gpio_list.info)[i].PinNum;
				gpio_export(pinNum);
				if(gpio_set_dir(pinNum,(argp->gpio_list.info)[i].data) == 0)
					retval = 0;
				else {retval = -1;break;}
			}
			break;
		default:
			break;
	}


	return( retval );
}
/**
 * get_gpio_dir
 *
 **/
int get_gpio_dir( unsigned short gpio_number )
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio number */
	gpio_arg.PinNum = gpio_number;

	return( gpio_action( &gpio_arg, GET_GPIO_DIRECTION ) );
}


/**
 * set_gpio_dir 
 *
 **/
static int set_gpio_dir( unsigned short gpio_number,
		unsigned char gpio_direction )
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio number */
	gpio_arg.PinNum = gpio_number;

	/* Set the gpio direction */
	gpio_arg.data = gpio_direction;

	return( gpio_action( &gpio_arg, SET_GPIO_DIRECTION ) );

}


/**
 * set_gpio_dir_intput
 * set the gpio as input 
 **/

inline int set_gpio_dir_input( unsigned short gpio_number )
{
	return( set_gpio_dir( gpio_number, (unsigned char)0) );
}

/**
 * set_gpio_dir_output
 * set the gpio as output 
 **/
inline int set_gpio_dir_output( unsigned short gpio_number )
{
	return( set_gpio_dir( gpio_number, (unsigned char)1 ) );
}

/**
 * get_gpio_pol
 * get the gpio polarity 
 **/
int get_gpio_pol ( unsigned short gpio_number )
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio number */
	gpio_arg.PinNum = gpio_number;

	return( gpio_action( &gpio_arg, GET_GPIO_POLARITY ) );

}

/**
 * set_gpio_pol
 * set the gpio polarity 
 **/
static int set_gpio_pol ( unsigned short gpio_number, unsigned char gpio_data )
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio number */
	gpio_arg.PinNum = gpio_number;

	/* Set the gpio data */
	gpio_arg.data = gpio_data;

	return( gpio_action( &gpio_arg, SET_GPIO_POLARITY ) );
}

/**
 * set_gpio_pol_high
 * set the gpio polarity high 
 **/
inline int set_gpio_pol_high( unsigned short gpio_number )
{
	return( set_gpio_pol ( gpio_number, (unsigned char )1 ) );
}

/**
 * set_gpio_low
 * set the gpio polarity low
 **/
inline int set_gpio_pol_low( unsigned short gpio_number )
{
	return( set_gpio_pol ( gpio_number, (unsigned char )0 ));
}

/**
 * get_gpio_pull_up_down
 * get the gpio pull up/down control bits. 
 **/
int get_gpio_pull_up_down ( unsigned short gpio_number )
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio number */
	gpio_arg.PinNum = gpio_number;

	return( gpio_action( &gpio_arg, GET_GPIO_PULL_UP_DOWN ) );

}

/**
 * set_gpio_pull_up_down
 * set the gpio pull up/down control bits.  
 **/
static int set_gpio_pull_up_down ( unsigned short gpio_number, unsigned char gpio_data )
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio number */
	gpio_arg.PinNum = gpio_number;

	/* Set the gpio data */
	gpio_arg.data = gpio_data;

	return( gpio_action( &gpio_arg, SET_GPIO_PULL_UP_DOWN ) );
}

/**
 * set_gpio_pull_down
 * Enable internal pull-up
 **/
inline int set_gpio_pull_down( unsigned short gpio_number )
{
	return( set_gpio_pull_up_down( gpio_number, (unsigned char)GPIO_ENABLE_PULL_DOWN ) );
}

/**
 * set_gpio_pull_up
 * Enable internal pull-up
 **/
inline int set_gpio_pull_up( unsigned short gpio_number )
{
	return( set_gpio_pull_up_down( gpio_number, (unsigned char)GPIO_ENABLE_PULL_UP ) );
}

/**
 * set_gpio_no_pull_up_down
 * Enable internal pull-up
 **/
inline int set_gpio_no_pull_up_down( unsigned short gpio_number )
{
	return( set_gpio_pull_up_down( gpio_number, (unsigned char)GPIO_DISABLE_PULL_UP_DOWN ) );
}


/**
 * get_gpio_data
 * read the gpio data
 **/
int get_gpio_data( unsigned short gpio_number )
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio number */
	gpio_arg.PinNum = gpio_number;

	return( gpio_action( &gpio_arg, READ_GPIO ) );

}

/**
 * set_gpio_data
 * write the gpio data
 **/
static int set_gpio_data( unsigned short gpio_number, unsigned char gpio_data )
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio number */
	gpio_arg.PinNum = gpio_number;

	/* Set the gpio data */
	gpio_arg.data = gpio_data;

	return( gpio_action( &gpio_arg, WRITE_GPIO ) );
}

/**
 * set_gpio_data_high
 * set as high
 **/
inline int set_gpio_data_high( unsigned short gpio_number )
{
	return( set_gpio_data( gpio_number, (unsigned char)1 ) );
}


/**
 * set_gpio_data_low
 * set as low
 **/
inline int set_gpio_data_low( unsigned short gpio_number )
{
	return( set_gpio_data( gpio_number, (unsigned char)0 ) );
}

/**
 * set_gpio_data
 * write the gpio data
 **/
int set_gpio_od_output( unsigned short gpio_number, unsigned char gpio_data )
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio number */
	gpio_arg.PinNum = gpio_number;

	/* Set the gpio data */
	gpio_arg.data = gpio_data;

	return( gpio_action( &gpio_arg, SET_GPIO_OD_OUT ) );
}

/**
 * set_gpio_od_output_high
 * set as high
 **/
inline int set_gpio_od_output_high( unsigned short gpio_number )
{
	return( set_gpio_od_output( gpio_number, (unsigned char)1 ) );
}

/**
 * set_gpio_od_output_low
 * set as low
 **/
inline int set_gpio_od_output_low( unsigned short gpio_number )
{
	return( set_gpio_od_output( gpio_number, (unsigned char)0 ) );
}
/**
 * set_sgpio_cont_mode_high
 * write the cont mode high
 **/
static int set_sgpio_cont_mode( unsigned short gpio_number, unsigned char gpio_data )
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio number */
	gpio_arg.PinNum = gpio_number;

	/* Set the gpio data */
	gpio_arg.data = gpio_data;
	return( gpio_action( &gpio_arg, SET_CONT_MODE ) );
}


inline int set_sgpio_cont_mode_high( unsigned short gpio_number )
{
	return( set_sgpio_cont_mode( gpio_number, (unsigned char)1 ) );
}

int get_gpio_list_data (gpio_list_data_info *info, unsigned int count)
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio data count */
	gpio_arg.gpio_list.count = count;

	/* Set the gpio data list */
	gpio_arg.gpio_list.info = info;
	return( gpio_action( &gpio_arg, READ_GPIOS) );
}

int set_gpio_list_data (gpio_list_data_info *info, unsigned int count)
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio data count */
	gpio_arg.gpio_list.count = count;

	/* Set the gpio data list */
	gpio_arg.gpio_list.info = info;
	return( gpio_action( &gpio_arg, WRITE_GPIOS) );
}

int get_gpio_list_dir (gpio_list_data_info *info, unsigned int count)
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio data count */
	gpio_arg.gpio_list.count = count;

	/* Set the gpio data list */
	gpio_arg.gpio_list.info = info;
	return( gpio_action( &gpio_arg, GET_GPIOS_DIR) );
}

int set_gpio_list_dir (gpio_list_data_info *info, unsigned int count)
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio data count */
	gpio_arg.gpio_list.count = count;

	/* Set the gpio data list */
	gpio_arg.gpio_list.info = info;
	return( gpio_action( &gpio_arg, SET_GPIOS_DIR) );
}

/**
 * get_gpios_data
 * read the gpios data
 **/
int get_gpios_data( int count, unsigned short *gpio_number, unsigned char *result )
{
	int i = 0;
	int retval = 0;
	gpio_list_data_info *gpios_info = NULL;
	size_t size = 0;
	/* Integer overflow, which can result in a logic error or a buffer overflow */
	if ( (size = (sizeof(gpio_list_data_info) * count) ) > INT_MAX)
	{
		printf("Integer Overflow \n ");
		return -1;
	} 
	gpios_info = (gpio_list_data_info *)malloc(size);
	if (gpios_info == NULL)
	{
		printf("Unable to send the pin info\n");
		return -1;
	}

	for (i=0; i<count; i++)
	{
		(gpios_info[i]).PinNum = (unsigned short)(gpio_number[i]);
	}

	retval = get_gpio_list_data (gpios_info, count);

	if (retval < 0)
	{
		printf("Error in fetching the gpio info\n");
		retval = -1;
		goto error_out;
	}

	for (i=0; i<count; i++)
	{
		result[i] = (gpios_info[i]).data;	/* Fortify [Buffer overflow]:: False Positive */
	}

error_out:
	if (gpios_info)
	{
		free (gpios_info);
		gpios_info = NULL;
	}

	return retval;
}

/**
 * set_gpios_data
 * set the gpios data as high or low depending on data
 **/
int set_gpios_data( int count, unsigned short *gpio_number, int data )
{
	int i = 0;
	int retval = 0;
	gpio_list_data_info *gpios_info = NULL;
	size_t size = 0;
	/* Integer overflow, which can result in a logic error or a buffer overflow */
	if ( (size = (sizeof(gpio_list_data_info) * count) ) > INT_MAX)
	{
		printf("Integer Overflow \n ");
		return -1;
	}
	gpios_info = (gpio_list_data_info *)malloc(size);
	if (gpios_info == NULL)
	{
		printf("Unable to send the pin info\n");
		return -1;
	}

	for (i=0; i<count; i++)
	{
		(gpios_info[i]).PinNum = (unsigned short)(gpio_number[i]);
		(gpios_info[i]).data = data;
	}

	retval = set_gpio_list_data (gpios_info, count);

	if (retval < 0)
	{
		printf("Error in setting the gpio info\n");
	}

	if (gpios_info)
	{
		free (gpios_info);
		gpios_info = NULL;
	}

	return retval;
}

/**
 * get_gpios_dir
 * read the gpios dir
 **/
int get_gpios_dir( int count, unsigned short *gpio_number, unsigned char *result )
{
	int i = 0;
	int retval = 0;
	gpio_list_data_info *gpios_info = NULL;
	size_t size = 0;
	/* Integer overflow, which can result in a logic error or a buffer overflow */
	if ( (size = (sizeof(gpio_list_data_info) * count) ) > INT_MAX)
	{
		printf("Integer Overflow \n ");
		return -1;
	}

	gpios_info = (gpio_list_data_info *)malloc(size);
	if (gpios_info == NULL)
	{
		printf("Unable to send the pin info\n");
		return -1;
	}

	for (i=0; i<count; i++)
	{
		(gpios_info[i]).PinNum = (unsigned short)(gpio_number[i]);
	}

	retval = get_gpio_list_dir (gpios_info, count);

	if (retval < 0)
	{
		printf("Error in fetching the gpio info\n");
		retval = -1;
		goto error_out;
	}

	for (i=0; i<count; i++)
	{
		result[i] = (gpios_info[i]).data;	/* Fortify [Buffer overflow]:: False Positive */
	}

error_out:
	if (gpios_info)
	{
		free (gpios_info);
		gpios_info = NULL;
	}

	return retval;
}

/**
 * set_gpios_dir
 * set the gpios dir as input or output depending on dir
 **/
int set_gpios_dir( int count, unsigned short *gpio_number, int dir )
{
	int i = 0;
	int retval = 0;
	gpio_list_data_info *gpios_info = NULL;
	size_t size = 0;
	/* Integer overflow, which can result in a logic error or a buffer overflow */
	if ( (size = (sizeof(gpio_list_data_info) * count) ) > INT_MAX)
	{
		printf("Integer Overflow \n ");
		return -1;
	}
	gpios_info = (gpio_list_data_info *)malloc(size);
	if (gpios_info == NULL)
	{
		printf("Unable to send the pin info\n");
		return -1;
	}

	for (i=0; i<count; i++)
	{
		(gpios_info[i]).PinNum = (unsigned short)(gpio_number[i]);
		(gpios_info[i]).data = dir;
	}

	retval = set_gpio_list_dir (gpios_info, count);

	if (retval < 0)
	{
		printf("Error in setting the gpio info\n");
	}

	if (gpios_info)
	{
		free (gpios_info);
		gpios_info = NULL;
	}

	return retval;
}

/**
 ** @fn set_gpio_property
 ** @brief  To set the gpio property.
 ** @param gpionum         - gpio number for which property is to be set
 **                          use depending on property id.
 ** @param property_id     - property id
 ** @param property_value  - value
 ** @retval   Will return 0 on success.
 **
 **/
inline int set_gpio_property( unsigned short gpionum, unsigned char property_id, unsigned char property_value)
{
	gpio_ioctl_data gpio_arg;
	/* Set the gpio data */
	gpio_arg.PinNum = gpionum;
	gpio_arg.property.id = property_id;
	gpio_arg.data = property_value;
	return( gpio_action( &gpio_arg, SET_GPIO_PROPERTY ) );
}
/**
 ** @fn get_gpio_property
 ** @brief  To get the gpio property.
 ** @param gpionum         - gpio number for which property is to be set
 **                          use depending on property id.
 ** @param property_id     - property id
 ** @param property_value  - pointer to property value.
 ** @retval   Will return 0 on success.
 **
 **/
inline int get_gpio_property( unsigned short gpionum, unsigned char property_id, unsigned char * property_value)
{
	int retval;
	gpio_ioctl_data gpio_arg;
	memset(&gpio_arg, 0, sizeof(gpio_ioctl_data));
	gpio_arg.PinNum = gpionum;
	gpio_arg.property.id = property_id;
	retval = gpio_action( &gpio_arg, GET_GPIO_PROPERTY );
	if(retval != -1)
	{
		*property_value = gpio_arg.data;
		return 0;
	}
	return retval;
}


/**
 * get_gpio_debounce
 * get the gpio debounce event
 **/
int get_gpio_debounce ( unsigned short gpio_number )
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio number */
	gpio_arg.PinNum = gpio_number;

	return( gpio_action( &gpio_arg, GET_GPIO_DEBOUNCE ) );

}

/**
 * set_gpio_debounce
 * set the gpio debounce event 
 **/
int set_gpio_debounce ( unsigned short gpio_number, unsigned char gpio_debouncing )
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio number */
	gpio_arg.PinNum = gpio_number;

	/* Set the gpio data */
	gpio_arg.data = gpio_debouncing;

	return( gpio_action( &gpio_arg, SET_GPIO_DEBOUNCE ) );
}

/**
 * enable_gpio_debounce
 * enables gpio debouncing
 **/
int enable_gpio_debounce( unsigned short gpio_number )
{
	return( set_gpio_debounce ( gpio_number, (unsigned char )1 ) );
}

/**
 * disable_gpio_debounce
 * disables gpio debouncing
 **/
int disable_gpio_debounce( unsigned short gpio_number )
{
	return( set_gpio_debounce ( gpio_number, (unsigned char )0 ));
}

/**
 * get_gpio_debounce_clock
 * get the gpio debounce clock
 **/
int get_gpio_debounce_clock ( unsigned short gpio_number )
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio number */
	gpio_arg.PinNum = gpio_number;

	return( gpio_action( &gpio_arg, GET_GPIO_DEBOUNCE_CLOCK ) );

}

/**
 * set_gpio_debounce_clock
 * set the gpio debounce clock 
 **/
int set_gpio_debounce_clock ( unsigned short gpio_number, unsigned char gpio_debounce_clock )
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio number */
	gpio_arg.PinNum = gpio_number;

	/* Set the gpio data */
	gpio_arg.data = gpio_debounce_clock;

	return( gpio_action( &gpio_arg, SET_GPIO_DEBOUNCE_CLOCK ) );
}

/**
 * pulse_gpio_data
 * send a pulse request for the specified gpio number
 **/
static int pulse_gpio_data( unsigned short gpio_number, unsigned char gpio_data )
{
	gpio_ioctl_data gpio_arg;

	/* Set the gpio number */
	gpio_arg.PinNum = gpio_number;

	/* Set the gpio data */
	gpio_arg.data = gpio_data;

	return( gpio_action( &gpio_arg, PULSE_GPIO ) );
}

/**
 * pulse_gpio_data_high
 * request a pulse by rising the edge (and then dropping it)
 **/
inline int pulse_gpio_data_high( unsigned short gpio_number )
{
	return( pulse_gpio_data( gpio_number, (unsigned char)1 ) );
}

/**
 * pulse_gpio_data_low
 * request a pulse by dropping the edge (and then rising it)
 **/
inline int pulse_gpio_data_low( unsigned short gpio_number )
{
	return( pulse_gpio_data( gpio_number, (unsigned char)0 ) );
}
