#ifndef __WACOM_I2C_H__
#define __WACOM_I2C_H__

struct wacom_i2c_platform_data {
	int		irq_gpio;
	int		power_gpio;
	int		reset_gpio;
	int		invert_x;
	int		invert_y;
	int		swap_xy;
};

extern int wacom_i2c_is_active(void);

#endif // !__WACOM_I2C_H__
