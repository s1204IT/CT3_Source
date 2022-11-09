#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "cust_settings.h"
#include "settings.h"
#include "settings.h"
#include "debug.h"

#define PERSIST_PROPERTY_PREFIX "persist."

static cust_setting_t cust_settings[SETTING_END] = {
	[IKE_INTERFACE]           = {"net.wo.ikev2if",       {.str = "wlan0"}},
	[IKE_PORT]                = {"net.wo.port",          {.integer = -1}},
	[IKE_PORT_NATT]           = {"net.wo.port_natt",     {.integer = -1}},
	[ENCR_KEY_DISPLAY]        = {"net.wo.key.display",   {.boolean = true}},
	[WSHARK_KEY_DUMP]         = {"net.wo.key.dump",      {.boolean = false}},
};

static int strcicmp(char const *a, char const *b)
{
	for (;; a++, b++)
	{
		int d = tolower(*a) - tolower(*b);

		if (d != 0 || !*a)
		{
			return d;
		}
    }
}

#define strcieq(s, t) (strcicmp(s, t) == 0)

static inline bool str_to_bool(const char* v)
{
	if (!v)
	{
		return false;
	}

	if (strcieq(v, "true") || strcieq(v, "1") || strcieq(v, "yes") 
							|| strcieq(v, "y") || strcieq(v, "on"))
	{
		return true;
	}

	return false;
}

static inline const char* get_key(cust_setting_type_t type)
{
	return cust_settings[type].system_property_key;
}

static inline cust_value_t get_default(cust_setting_type_t type)
{
	return cust_settings[type].default_value;
}

int get_cust_setting(cust_setting_type_t type, char* value)
{
	const char *key = get_key(type);
	const char *default_value = get_default(type).str;
	int len = 0;

#ifdef __ANDROID__
	if (key)
	{
		char persist_property_name[PROP_NAME_MAX];

		snprintf(persist_property_name, PROP_NAME_MAX, "%s%s",
									PERSIST_PROPERTY_PREFIX, key);
		if ((len = __system_property_get(persist_property_name, value)))
		{
			return len;
		}
		len = property_get(key, value, default_value);
	}
	else if (default_value)
	{
#endif
		len = strlen(default_value);
		sprintf(value, default_value);
#ifdef __ANDROID__
	}
#endif

	return len;
}

bool get_cust_setting_bool(cust_setting_type_t type)
{
	const char *key = get_key(type);
	bool ret = get_default(type).boolean;

#ifdef __ANDROID__
	if (key)
	{
		char value[PROP_VALUE_MAX];
		char persist_property_name[PROP_NAME_MAX];

		snprintf(persist_property_name, PROP_NAME_MAX, "%s%s",
									PERSIST_PROPERTY_PREFIX, key);
		if (__system_property_get(persist_property_name, value))
			ret = str_to_bool(value);
		else
			ret = property_get_bool(key, ret);
	}
#endif

	return ret;
}

int get_cust_setting_int(cust_setting_type_t type)
{
	const char *key = get_key(type);
	int ret = get_default(type).integer;

#ifdef __ANDROID__
	if (key)
	{
		char value[PROP_VALUE_MAX];
		char persist_property_name[PROP_NAME_MAX];

		snprintf(persist_property_name, PROP_NAME_MAX, "%s%s",
									PERSIST_PROPERTY_PREFIX, key);
		if (__system_property_get(persist_property_name, value))
		{
			ret = atoi(value);
		}
		else if (__system_property_get(key, value))
		{
			ret = atoi(value);
		}
	}
#endif

	return ret;
}

int set_cust_setting(cust_setting_type_t type, char* value)
{
	const char *key = get_key(type);
	int ret = -1;

#ifdef __ANDROID__
	if (key)
	{
		ret = __system_property_set(key, value);
	}
#endif

	return ret;
}

int set_cust_setting_bool(cust_setting_type_t type, bool value)
{
	const char *key = get_key(type);
	int ret = -1;

#ifdef __ANDROID__
	if (key)
	{
		if (value)
		{
			ret = __system_property_set(key, "1");
		}
		else
		{
			ret = __system_property_set(key, "0");
		}
	}
#endif

	return ret;
}

int set_cust_setting_int(cust_setting_type_t type, int value)
{
	const char *key = get_key(type);
	int ret = -1;

#ifdef __ANDROID__
	if (key)
	{
		char p_value[PROP_VALUE_MAX];

		snprintf(p_value, PROP_VALUE_MAX, "%d", value);
		ret = __system_property_set(key, p_value);
	}
#endif

	return ret;
}

