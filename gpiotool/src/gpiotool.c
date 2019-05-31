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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <stdint.h>
#include "gpioifc.h"
#include <gpio.h>

typedef enum {
	GET_GPIO_DIR,
	SET_GPIO_DIR_IN,
	SET_GPIO_DIR_OUT,
	GET_GPIO_POL,
	SET_GPIO_POL_HIGH,
	SET_GPIO_POL_LOW,
	GET_GPIO_PULL_UP_DOWN_ACTION,
	SET_GPIO_PULL_DOWN,
	SET_GPIO_PULL_UP,
	SET_GPIO_NO_PULL_UP_DOWN,
	READ_GPIO_VAL,
	SET_GPIO_HIGH,
	SET_GPIO_LOW,
	SET_GPIO_OD_HIGH,
	SET_GPIO_OD_LOW,
	ENABLE_GPIO_DEBOUNCE,
	DISABLE_GPIO_DEBOUNCE,
	GET_GPIO_DEBOUNCE_EVENT,
	TOOL_SET_GPIO_DEBOUNCE_CLOCK,
	TOOL_GET_GPIO_DEBOUNCE_CLOCK,
	GET_GPIOS_DATA,
	SET_GPIOS_HIGH,
	SET_GPIOS_LOW,
	GET_GPIOS_DIR_ACTION,
	SET_GPIOS_DIR_INPUT,
	SET_GPIOS_DIR_OUTPUT,
	END_OF_FUNCLIST
}eGpioactions;

#define GPIO_INTERNAL_PULL_DOWN_ENABLE  0
#define GPIO_INTERNAL_PULL_UP_ENABLE    1
#define GPIO_NO_PULL_UP_PULL_DOWN       2
#define GPIO_INTERNAL_PULL_DOWN_DISABLE 3

#define ARG_FILENAME	1
#define ARG_OPTION	1

eGpioactions action = END_OF_FUNCLIST;

static int verbose = 1;
static int fdesc;

static void ShowUsuage ( void )
	/*@globals fileSystem@*/
	/*@modifies fileSystem@*/
{
	printf ("Gpio Test Tool - Copyright (c) 2009-2015 American Megatrends Inc.\n");	
	printf( "Usage : gpiotool <gpionumber> <option> [--verbose]\n" );
	printf( "\t--get-dir:       \tGet the specified gpio's direction\n" );
	printf( "\t--set-dir-input: \tSet the specified gpio to be an input\n" );
	printf( "\t--set-dir-output:\tSet the specified gpio to be an output\n" );

	printf( "\t--get-pol:       \tGet the specified gpio polarity\n" );
	printf( "\t--set-pol-high:  \tSet the specified gpio polarity\n" );
	printf( "\t--set-pol-low:   \tSet the specified gpio polarity\n" );

	printf( "\t--get-pull-up-down:	Get the specified gpio pull-up/pull-down control bits\n" );
	printf( "\t--set-pull-down:  \tSet Internal Pull-down enabled for specified gpio\n" );
	printf( "\t--set-pull-up:    \tSet Internal Pull-up enabled for specified gpio\n" );
	printf( "\t--set-no-pull-up-down:	Set No pull-up/Pull-down for the specified gpio\n" );

	printf( "\t--get-gpios-data:     \tGet the data for multiple gpios, with spaces between each number\n" );
	printf( "\t--set-gpios-high:     \tSet the data to high for multiple gpios, with spaces between each number\n" );
	printf( "\t--set-gpios-low:      \tSet the data to low for multiple gpios, with spaces between each number\n" );
	printf( "\t--get-gpios-dir:      \tGet the direction for multiple gpios, with spaces between each number\n" );
	printf( "\t--set-gpios-dir-input:	Set the direction to input for multiple gpios, with spaces between each number\n" );
	printf( "\t--set-gpios-dir-output:	Set the direction to output for multiple gpios, with spaces between each number\n" );

	printf( "\t--get-data:      \tGet the specified gpio's data\n" );	
	printf( "\t--set-data-high: \tSet the specified gpio high\n" );
	printf( "\t--set-data-low:  \tSet the specified gpio low\n" );
	printf( "\t--set-od-high:   \tSet the specified gpio as open drain output line with high level\n" );
	printf( "\t--set-od-low:    \tSet the specified gpio as open drain output line with low level\n" );
	printf( "\t--enable-debouncing: \tEnable GPIO Debouncing\n" );
	printf( "\t--disable-debouncing: \tDisable GPIO Debouncing\n" );
	printf( "\t--get-debounce-event: \tGet GPIO Debounce event\n" );
	printf( "\t--get-debounce-clock: \tGet GPIO Debounce clock value\n" );
	printf( "\t--set-debounce-clock: \tset GPIO Debounce clock value\n" );
	printf( "\t--verbose:       \tVerbose for Debugging messages \n" );
	printf( "\n" );
}

	static void  
Verbose ( char * msg )
{
	if (verbose ) printf ( "%s\n" , msg );

}

static int process_arguments( int argc, char **argv,
		unsigned short *gpio_num )
{
	int i = 1;
	*gpio_num = 0 ;
	while( i < argc )
	{
		/* Check for a command */
		if( strcmp( argv[ i ], "--get-dir" ) == 0 )  
		{
			action = GET_GPIO_DIR;
		}
		else if( strcmp( argv[ i ], "--set-dir-input" ) == 0 )
		{
			action = SET_GPIO_DIR_IN;
		}
		else if( strcmp( argv[ i ], "--set-dir-output" ) == 0 )
		{
			action = SET_GPIO_DIR_OUT;
		}
		else if( strcmp( argv[ i ], "--get-pol" ) == 0 )
		{
			action = GET_GPIO_POL;
		}
		else if( strcmp( argv[ i ], "--set-pol-high" ) == 0 )
		{
			action = SET_GPIO_POL_HIGH;
		}
		else if( strcmp( argv[ i ], "--set-pol-low" ) == 0 )
		{
			action = SET_GPIO_POL_LOW;
		}
		else if( strcmp( argv[ i ], "--get-pull-up-down" ) == 0 )
		{
			action = GET_GPIO_PULL_UP_DOWN_ACTION;
		}
		else if( strcmp( argv[ i ], "--set-pull-down" ) == 0 )
		{
			action = SET_GPIO_PULL_DOWN;
		}
		else if( strcmp( argv[ i ], "--set-pull-up" ) == 0 )
		{
			action = SET_GPIO_PULL_UP;
		}
		else if( strcmp( argv[ i ], "--set-no-pull-up-down" ) == 0 )
		{
			action = SET_GPIO_NO_PULL_UP_DOWN;
		}
		else if( strcmp( argv[ i ], "--get-data" ) == 0 )
		{
			action = READ_GPIO_VAL;
		}
		else if( strcmp( argv[ i ], "--set-data-low" ) == 0 )
		{
			action = SET_GPIO_LOW;
		}
		else if( strcmp( argv[ i ], "--set-data-high" ) == 0 )	
		{
			action = SET_GPIO_HIGH;
		}
		else if( strcmp( argv[ i ], "--set-od-high" ) == 0 )	
		{
			action = SET_GPIO_OD_HIGH;
		}
		else if( strcmp( argv[ i ], "--set-od-low" ) == 0 )	
		{
			action = SET_GPIO_OD_LOW;
		}
		else if( strcmp( argv[ i ], "--enable-debouncing" ) == 0 )
		{
			action = ENABLE_GPIO_DEBOUNCE;
		}	
		else if( strcmp( argv[ i ], "--disable-debouncing" ) == 0 )
		{
			action = DISABLE_GPIO_DEBOUNCE;
		}	
		else if( strcmp( argv[ i ], "--get-debounce-event" ) == 0 )
		{
			action = GET_GPIO_DEBOUNCE_EVENT;
		}
		else if( strcmp( argv[ i ], "--get-debounce-clock" ) == 0 )
		{
			action = TOOL_GET_GPIO_DEBOUNCE_CLOCK;
		}
		else if( strcmp( argv[ i ], "--set-debounce-clock" ) == 0 )
		{
			action = TOOL_SET_GPIO_DEBOUNCE_CLOCK;
		}
		else if ( strcmp( argv[ i ], "--get-gpios-data" ) == 0 )
		{
			action = GET_GPIOS_DATA;
		}
		else if ( strcmp( argv[ i ], "--set-gpios-high" ) == 0 )
		{
			action = SET_GPIOS_HIGH;
		}
		else if ( strcmp( argv[ i ], "--set-gpios-low" ) == 0 )
		{
			action = SET_GPIOS_LOW;
		}
		else if ( strcmp( argv[ i ], "--get-gpios-dir" ) == 0 )
		{
			action = GET_GPIOS_DIR_ACTION;
		}
		else if ( strcmp( argv[ i ], "--set-gpios-dir-input" ) == 0 )
		{
			action = SET_GPIOS_DIR_INPUT;
		}
		else if ( strcmp( argv[ i ], "--set-gpios-dir-output" ) == 0 )
		{
			action = SET_GPIOS_DIR_OUTPUT;
		}
		else if( strcmp( argv[ i ], "--verbose" ) == 0 )
			verbose = 1;
		else
		{
			/* If not a command, it should be a gpio */
			*gpio_num = (unsigned short)strtol( argv[ i ], NULL, 10);
			gpio_num ++;
			//printf ("The pin No is %d\n\n",*gpio_num );
		}
		i++;
	}
	return 0;
}

int
main ( int argc , char* argv [] )
{
	unsigned short *gpionum = NULL;
	unsigned char *result = NULL;
	unsigned long va_address;
	int Value   = 0;
	int i = 0;
	char *datastr = NULL;
	int retval = 0;

	if ( !(argc >= 3 ) )
	{
		ShowUsuage () ; 
		return 0;	
	}

	// allocate a buffer to store the gpio numbers
	// The number of gpio numbers given in command line is total - 1 for filename - 1 for option
	gpionum = malloc(sizeof(unsigned short) * (argc - ARG_FILENAME - ARG_OPTION));
	if (gpionum == NULL)
	{
		printf("Unable to parse the arguments\n");
		return -1;
	}

	process_arguments( argc , argv , (unsigned short *)gpionum );
	if ( (END_OF_FUNCLIST == action)  )
	{
		ShowUsuage ();
		retval = 0;
		goto error_out;	
	}

	switch ( action )
	{
		case GET_GPIO_DIR:
			Verbose   ("Inside Get Dir\n");
			Value = get_gpio_dir ( (unsigned short)*gpionum );
			if  ( -1 == Value )
			{
				printf ( "Get Dir Failed \n"); 
				retval = -1;
				goto error_out;
			}	
			//printf ( "Dir:%d\n",Value);
			if ( 1 == Value ) 	printf ( "Output Pin\n");
			else if ( 0 == Value ) 	printf ( "Input Pin\n");
			else if ( 2 == Value ) 	printf ( "Open Drain Output Pin\n");
			break;
		case SET_GPIO_DIR_IN:
			Verbose   ("Inside Set as Input \n");
			if  ( -1 == set_gpio_dir_input ((unsigned short)*gpionum ))
			{
				printf ( "Set Dir in Failed \n"); 
				retval = -1;
				goto error_out;
			}	
			Verbose   ("GPIO pin is set as input pin \n");
			break;
		case SET_GPIO_DIR_OUT:
			Verbose   ("Inside Set as Output \n");
			if ( -1 == set_gpio_dir_output ((unsigned short)*gpionum ) )
			{
				printf ( "Set Dir out Failed \n"); 
				retval = -1;
				goto error_out;
			}	
			Verbose   ("GPIO pin is set as output pin \n");
			break;		
		case GET_GPIO_POL:
			Verbose   ("Inside Get GPIO Pol \n");
			Value = get_gpio_pol ((unsigned short)*gpionum );
			if  ( -1 == Value )
			{
				printf ( "Get Pol Failed \n"); 
				retval = -1;
				goto error_out;
			}	
			printf ( "Pol is %d\n", Value );
			if ( 1 == Value ) 	printf ( "High Pol \n");
			else if ( 0 == Value ) 	printf ( "Low  Pol\n");
			break;	
		case SET_GPIO_POL_HIGH:
			Verbose   ("Inside Set Pol high \n");
			if ( -1 == set_gpio_pol_high ((unsigned short)*gpionum ) )
			{
				printf ( "Set Pol high Failed \n"); 
				retval = -1;
				goto error_out;
			}	
			Verbose  ("GPIO pol is set to high \n");
			break;		
		case SET_GPIO_POL_LOW:
			Verbose   ("Inside Set Pol low \n");
			if ( -1 == set_gpio_pol_low ((unsigned short)*gpionum ) )
			{
				printf ( "Set Pol low Failed \n"); 
				retval = -1;
				goto error_out;
			}	
			Verbose  ("GPIO pol is set to low \n");
			break;
		case GET_GPIO_PULL_UP_DOWN_ACTION:
			Verbose   ("Inside Get GPIO Pull-up/Pull-down \n");
			Value = get_gpio_pull_up_down ((unsigned short)*gpionum );
			if  ( -1 == Value )
			{
				printf ( "Get Pull-up/Pull-down Failed \n");
				retval = -1;
				goto error_out;
			}

			if ( GPIO_INTERNAL_PULL_DOWN_ENABLE == Value )
			{
				printf ( "Internal pull-down enable \n");
			}
			else if ( GPIO_INTERNAL_PULL_UP_ENABLE == Value )
			{
				printf ( "Internal pull-up enable\n");
			}
			else if ( GPIO_NO_PULL_UP_PULL_DOWN == Value )
			{
				printf ( "No pull-up/pull-down enable\n");
			}
			else if(GPIO_INTERNAL_PULL_DOWN_DISABLE == Value)
			{
				printf(  "Internal pull-down disable \n");
			}
			break;
		case SET_GPIO_PULL_DOWN:
			Verbose   ("Inside Set Pull-down enable \n");
			if ( -1 == set_gpio_pull_down ((unsigned short)*gpionum ) )
			{
				printf ( "Set Pull-down enable Failed \n"); 
				retval = -1;
				goto error_out;
			}   
			Verbose  ("Internal pull-down enabled\n");
			break;      
		case SET_GPIO_PULL_UP:
			Verbose   ("Inside Set Pull-up enable \n");
			if ( -1 == set_gpio_pull_up ((unsigned short)*gpionum ) )
			{
				printf ( "Set Pull-up enable Failed \n"); 
				retval = -1;
				goto error_out;
			}   
			Verbose  ("Internal pull-up enabled\n");
			break;
		case SET_GPIO_NO_PULL_UP_DOWN:
			Verbose   ("Inside Set No Pull-up/Pull-down enable \n");
			if ( -1 == set_gpio_no_pull_up_down ((unsigned short)*gpionum ) )
			{
				printf ( "Set No Pull-up/Pull-down enable Failed \n"); 
				retval = -1;
				goto error_out;
			}   
			Verbose  ("No Pull-up/Pull-down enabled\n");
			break;
		case READ_GPIO_VAL:
			Verbose   ("Inside Read gpio.\n");
			Value = get_gpio_data ((unsigned short)*gpionum );
			if ( -1 == Value )
			{
				printf ( "Read Gpio failed\n");
				retval = -1;
				goto error_out;
			}
			//printf ( "The Pin value is %d\n", Value );
			if ( 1 == Value ) 	printf ( "Pin is High \n");
			else if ( 0 == Value ) 	printf ( "Pin is Low  \n");
			break;	

		case SET_GPIO_HIGH:
			Verbose   ("Inside Set Gpio high \n");
			if ( -1 == set_gpio_data_high  ((unsigned short)*gpionum ) )
			{
				printf ( "Set Gpio high failed\n");
				retval = -1;
				goto error_out;
			}
			Verbose   ("GPIO pin is set to high \n");
			break;		

		case SET_GPIO_LOW:
			Verbose   ("Inside Set Gpio low \n");
			if ( -1 == set_gpio_data_low ((unsigned short)*gpionum ) )
			{
				printf ( "Set Gpio low failed\n");
				retval = -1;
				goto error_out;
			}
			Verbose   ("GPIO pin is set to low \n");
			break;
		case SET_GPIO_OD_HIGH:
			Verbose   ("Inside Set Gpio OD high \n");
			if ( -1 == set_gpio_od_output_high  ((unsigned short)*gpionum ) )
			{
				printf ( "Set Gpio OD high failed\n");
				retval = -1;
				goto error_out;
			}
			Verbose   ("GPIO pin is set as OD high \n");
			break;		

		case SET_GPIO_OD_LOW:
			Verbose   ("Inside Set Gpio OD low \n");
			if ( -1 == set_gpio_od_output_low ((unsigned short)*gpionum ) )
			{
				printf ( "Set Gpio OD low failed\n");
				retval = -1;
				goto error_out;
			}
			Verbose   ("GPIO pin is set as OD low \n");
			break;
		case ENABLE_GPIO_DEBOUNCE:
			Verbose   ("Enabling Debounce event \n");
			if ( -1 == enable_gpio_debounce ((unsigned short)*gpionum ) )
			{
				printf ( "Enabling Debounce event Failed \n");
				retval = -1;
				goto error_out;
			}
			Verbose  ("GPIO Debouncing Enabled \n");
			break;
		case DISABLE_GPIO_DEBOUNCE:
			Verbose   ("Disabling Debounce event \n");
			if ( -1 == disable_gpio_debounce ((unsigned short)*gpionum ) )
			{
				printf ( "Disabling Debounce event Failed \n");
				retval = -1;
				goto error_out;
			}
			Verbose  ("GPIO Debouncing Disabled \n");
			break;
		case GET_GPIO_DEBOUNCE_EVENT:
			Verbose   ("Inside Get Debounce event \n");
			Value = get_gpio_debounce ((unsigned short)*gpionum );
			if ( -1 == Value )
			{
				printf ( "Get gpio debounce event Failed \n");
				retval = -1;
				goto error_out;
			}
			Verbose  ("Get gpio debounce event success\n");
			if ( 1 == Value )       printf ( "Debouncing Enabled\n");
			else if ( 0 == Value ) 	printf ( "Debouncing Disabled\n");

			break;
		case TOOL_GET_GPIO_DEBOUNCE_CLOCK:
			Verbose   ("Inside Get Debounce clock \n");
			Value = get_gpio_debounce_clock ((unsigned short)*gpionum );
			if ( -1 == Value )
			{
				printf ( "Get gpio debounce clock Failed \n");
				retval = -1;
				goto error_out;
			}
			Verbose  ("Get gpio debounce clock success\n");
			printf ("Debounce Value : %d\n", Value);

			break;

		case TOOL_SET_GPIO_DEBOUNCE_CLOCK:
			printf ("Enter the Clock Value : ");
			scanf ("%d", &Value);
			Value = set_gpio_debounce_clock ((unsigned short)*gpionum, Value );
			if ( -1 == Value )
			{
				printf ( "Set gpio debounce clock Failed \n");
				retval = -1;
				goto error_out;
			}
			break;
		case GET_GPIOS_DATA:
			Verbose ("Inside Get Gpios Data\n");
			result = malloc (sizeof(int) * (argc-2));
			if (result == NULL)
			{
				printf("Parsing the arguments failed\n");
				retval = -1;
				goto error_out;
			}

			Value = get_gpios_data ((argc - 2), gpionum, result);
			if ( -1 == Value )
			{
				printf ( "Get gpios data Failed \n");
				retval = -1;
				goto error_out;
			}
			Verbose  ("Get gpios data success\n");
			for (i=0; i<(argc-2); i++)
			{
				if ((int)(result[i]))
					datastr = "High";
				else
					datastr = "Low";

				printf("Pin %d is %s\n", (int)(gpionum[i]), datastr);
			}
			break;
		case SET_GPIOS_HIGH:
			Verbose ("Inside Set Gpios Data High\n");
			Value = set_gpios_data ((argc - 2), gpionum, 1);
			if ( -1 == Value )
			{
				printf ( "Set gpios data High Failed \n");
				retval = -1;
				goto error_out;
			}
			Verbose  ("Set gpios data high success\n");
			for (i=0; i<(argc-2); i++)
			{
				printf("Pin %d is set to High\n", (int)(gpionum[i]));
			} 
			break;
		case SET_GPIOS_LOW:
			Verbose ("Inside Set Gpios Data Low\n");
			Value = set_gpios_data ((argc - 2), gpionum, 0);
			if ( -1 == Value )
			{
				printf ( "Set gpios data Low Failed \n");
				retval = -1;
				goto error_out;
			}
			Verbose  ("Set gpios data Low success\n");
			for (i=0; i<(argc-2); i++)
			{
				printf("Pin %d is set to Low\n", (int)(gpionum[i]));
			} 
			break;
		case GET_GPIOS_DIR_ACTION:
			Verbose ("Inside Get Gpios Dir\n");
			result = malloc (sizeof(int) * (argc-2));
			if (result == NULL)
			{
				printf("Parsing the arguments failed\n");
				retval = -1;
				goto error_out;
			}

			Value = get_gpios_dir ((argc - 2), gpionum, result);
			if ( -1 == Value )
			{
				printf ( "Get gpios dir Failed \n");
				retval = -1;
				goto error_out;
			}
			Verbose  ("Get gpios dir success\n");
			for (i=0; i<(argc-2); i++)
			{
				if (result[i] == 0)
					datastr = "Input";
				else if (result[i] == 1)
					datastr = "Output";
				else if (result[i] == 2)
					datastr = "Open Drain Output";

				printf("Pin %d is %s\n", (int)(gpionum[i]), datastr);
			}
			break;
		case SET_GPIOS_DIR_INPUT:
			Verbose ("Inside Set Gpios Dir Input\n");
			Value = set_gpios_dir ((argc - 2), gpionum, 0);
			if ( -1 == Value )
			{
				printf ( "Set gpios dir Input Failed \n");
				retval = -1;
				goto error_out;
			}
			Verbose  ("Set gpios dir input success\n");
			for (i=0; i<(argc-2); i++)
			{
				printf("Pin %d is set to Input\n", (int)(gpionum[i]));
			} 
			break;
		case SET_GPIOS_DIR_OUTPUT:
			Verbose ("Inside Set Gpios Dir Output\n");
			Value = set_gpios_dir ((argc - 2), gpionum, 1);
			if ( -1 == Value )
			{
				printf ( "Set gpios dir Output Failed \n");
				retval = -1;
				goto error_out;
			}
			Verbose  ("Set gpios dir output success\n");
			for (i=0; i<(argc-2); i++)
			{
				printf("Pin %d is set to Output\n", (int)(gpionum[i]));
			} 
			break;
		default:
			Verbose  ("Invalid Gpio Function Call ");
			break;
	}

error_out:
	if (gpionum)
	{
		free (gpionum);
		gpionum = NULL;
	}

	if (result)
	{
		free (result);
		result = NULL;
	}

	return retval;
}
