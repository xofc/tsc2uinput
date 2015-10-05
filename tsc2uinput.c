/* tsc2uinput.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>

char *I2C_BUS_DEV = "/dev/i2c-1";

/* #include "bcm2835.h" */

/* See 
**	https://github.com/derekhe/wavesahre-7inch-touchscreen-driver
**	http://thiemonge.org/getting-started-with-uinput
*/
char *uinput_dev_str = "/dev/uinput";

/*
** calibration (perfect = 0 0 4096 4096)
*/
#define NOMINAL_CALIBRATION
#ifdef NOMINAL_CALIBRATION
#define MIN_X	0
#define MIN_Y	0
#define MAX_X	4096
#define MAX_Y	4096
#else
#define MIN_X	100
#define MIN_Y	300
#define MAX_X	3800
#define MAX_Y	3900
#endif

#define NOT_MUCH 200	/* small dx or dy ; about 3% of full width */
#define SHORT_CLICK 250	/* 250 ms */

void crash(char *str)
	{
	perror(str);
	exit(-1);
	}
int i2c_rw(int fd, int reg)
    {
    int    x;

    if ((x = i2c_smbus_read_word_data(fd, reg)) < 0)
        return(-1);
    return(((x&0xff)<<8) + ((x&0xff00)>>8));
    }
int now()
	{
	struct timeval tv;
	static int t0;

	if (gettimeofday(&tv, NULL) < 0)
		crash("gettimeofday()");
	if (t0 == 0)
		t0 = tv.tv_sec; /* first call */
	return((tv.tv_sec - t0)*1000 + tv.tv_usec/1000); /* Not foolproof... fails after 11 days up */
	}
int getxy(int fd, int *xp, int *yp)
	{
	int	z1, z2;

	*xp = (4096 - (i2c_rw(fd, 0xd4) >> 4));
	*yp = (4096 - (i2c_rw(fd, 0xc4) >> 4)); /* inverted: landscape x <> y & upside-down*/
	z1 = i2c_rw(fd, 0xe4);
	z2 = i2c_rw(fd, 0xf4);
	i2c_rw(fd, 0);
/*	printf("%6d %6d %6d %6d\n", (*xp), (*yp), z1>>4, z2>>4);	/**/
	return(z1 != 0);
	}

struct uinput_user_dev uidev;

int init_uinput()
	{
	int	fd;

	if ((fd = open(uinput_dev_str, O_WRONLY | O_NONBLOCK)) < 0)
		crash(uinput_dev_str);

	if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0
		|| ioctl(fd, UI_SET_KEYBIT, BTN_LEFT) < 0
		|| ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT) < 0
		|| ioctl(fd, UI_SET_EVBIT, EV_ABS) < 0
		|| ioctl(fd, UI_SET_ABSBIT, ABS_X) < 0
		|| ioctl(fd, UI_SET_ABSBIT, ABS_Y) < 0)
		crash("ioctl(UI_SET_*)");

	snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "hidraw2uinput");
	uidev.id.bustype = BUS_USB;
	uidev.id.vendor  = 0x1; uidev.id.product = 0x1; /* should be something else */
	uidev.id.version = 1;
	uidev.absmin[ABS_X] = MIN_X; uidev.absmax[ABS_X] = MAX_X;
	uidev.absmin[ABS_Y] = MIN_Y; uidev.absmax[ABS_Y] = MAX_Y;

	if (write(fd, &uidev, sizeof(uidev)) < 0)
		crash("write(&uidev)");

	if (ioctl(fd, UI_DEV_CREATE) < 0)
		crash("UI_DEV_CREATE");
	return(fd);
	}

int main (int argc, char **argv)
	{
	int	fd;
	int	x, y;
	int	uinput_fd;
	struct input_event     ev[2];
	int	pen_irq, touch_state, landing_x0, landing_y0, landing_t0, has_moved;

	if ((fd = open(I2C_BUS_DEV, O_RDWR)) < 0)
		crash(I2C_BUS_DEV);

	if (ioctl(fd, I2C_SLAVE, 0x48) < 0)  /* 0x1e = tsc2003 address */
		crash("I2C_SLAVE 0x1e tsc2003");

	uinput_fd = init_uinput();

	touch_state = 0; /* penUp */

	for (;;)
		{
		usleep(10000);	/* do it anyway ; settle time when pen goes up */
/*		pen_irq = bcm2835_gpio_lev(RPI_V2_GPIO_P1_22); /**/

		if (getxy(fd, &x, &y))
			{ /* P1.22 == PenIRQ is LOW : touch! pen is down */
			printf("%5d %5d\n", x, y);
			memset(&ev, 0, sizeof(struct input_event));
			ev[0].type = ev[1].type = EV_ABS;
			ev[0].code = ABS_X; ev[1].code = ABS_Y;
			ev[0].value = x; ev[1].value = y;
			if(write(uinput_fd,  &ev, sizeof(ev)) < 0)
				crash("event write");
			if (touch_state == 0)
				{ /* landing */
				touch_state = 1; has_moved = 0;
				landing_x0 = x; landing_y0 = y;
				landing_t0 = now();
				}
			else	{
				if (abs((x - landing_x0)) > NOT_MUCH || abs((y - landing_y0)) > NOT_MUCH)
					has_moved = 1;
				}
			}
		else	{ /* pen is up */
			if (touch_state == 1 && has_moved == 0)
				{ /* 'click' take-off */
				if ((now() - landing_t0) < SHORT_CLICK)
					{ /* short click == left mouse click */
					ev[0].type = EV_KEY;
					ev[0].code = BTN_LEFT;
					ev[0].value = 1;
					if(write(uinput_fd,  &ev[0], sizeof(ev[0])) < 0)
						crash("event write: Left-click");
					ev[0].value = 0;
					if(write(uinput_fd,  &ev[0], sizeof(ev[0])) < 0)
						crash("event write: Left-click");
					printf("left-click\n");
					}
				else	{ /* long click == right mouse click */
					ev[0].type = EV_KEY;
					ev[0].code = BTN_RIGHT;
					ev[0].value = 1;
					if(write(uinput_fd,  &ev[0], sizeof(ev[0])) < 0)
						crash("event write");
					ev[0].value = 0;
					if(write(uinput_fd,  &ev[0], sizeof(ev[0])) < 0)
						crash("event write: Right-click");
					printf("right-click: Right-click\n");
					}
				}
			touch_state = 0;
			}
		}
	
	bcm2835_close();

	exit(0);
	}
