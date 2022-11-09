#ifndef __WACOM__
#define __WACOM__

#define WACOM_SYSFS 1
#define ERR_REGISTER_SYSFS 1

struct wacom {
	struct device *dev;
	struct class *class;
	dev_t dev_t;
};

/*Added for Calibration purpose*/
typedef enum {
	STATE_NORMAL,
	STATE_QUERY,
	STATE_GETCAL,
	STATE_POINTS,
} NODE_STATE;

struct calibrationData {
  int   originX;
  int   originY;
  int   extentX;
  int   extentY; 
};

struct sPoint{
  int x;
  int y;
};
#endif
