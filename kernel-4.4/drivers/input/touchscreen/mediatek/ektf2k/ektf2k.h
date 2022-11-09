#ifndef _LINUX_ELAN_KTF2K_H
#define _LINUX_ELAN_KTF2K_H

#define ELAN_KTF2K_NAME "elan-ktf2k"

struct elan_ktf2k_i2c_platform_data {
	int intr_gpio;
	int rst_gpio;
	int onoff_sw_gpio;
};

#endif /* _LINUX_ELAN_KTF2K_H */
