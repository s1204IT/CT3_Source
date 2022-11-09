LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

MY_LOCAL_PATH := ${LOCAL_PATH}

# the executables that should be installed on the final system have to be added
# to PRODUCT_PACKAGES in
#   build/target/product/core.mk
# possible executables are
#   starter - allows to control and configure the daemon from the command line
#   charon - the IKE daemon
#   scepclient - SCEP client

# if you enable starter or scepclient (see above) uncomment the proper
# lines here
strongswan_BUILD_STARTER := true
# strongswan_BUILD_SCEPCLIENT := true

# this is the list of plugins that are built into libstrongswan and charon
# also these plugins are loaded by default (if not changed in strongswan.conf)
strongswan_CHARON_PLUGINS := openssl ctr fips-prf random nonce pubkey \
	pkcs1 pkcs8 pem xcbc hmac kernel-netlink socket-default \
	stroke updown eap-identity eap-sim eap-aka eap-simaka-reauth eap-mschapv2 eap-md5 eap-gtc xauth-generic attr resolve android-log \
	x509 revocation curl 

#android-dns android-log 

### from Makefile
#charon_plugins =  aes des rc2 sha1 sha2 md5 random nonce x509 revocation 
# constraints pubkey pkcs1 pkcs7 pkcs8 pkcs12 pgp dnskey sshkey pem fips-prf 
# gmp xcbc cmac hmac attr kernel-netlink resolve socket-default stroke updown 
# eap-identity eap-sim eap-sim-file eap-aka eap-simaka-reauth eap-mschapv2 xauth-generic

ifneq ($(strongswan_BUILD_SCEPCLIENT),)
# plugins loaded by scepclient
strongswan_SCEPCLIENT_PLUGINS := openssl curl fips-prf random pkcs1 pkcs7 pem
endif

strongswan_STARTER_PLUGINS := kernel-netlink

# list of all plugins - used to enable them with the function below
strongswan_PLUGINS := $(sort $(strongswan_CHARON_PLUGINS) \
			     $(strongswan_STARTER_PLUGINS) \
			     $(strongswan_SCEPCLIENT_PLUGINS))

include $(LOCAL_PATH)/Android.common.mk

# includes
strongswan_PATH := $(LOCAL_PATH)
#libcurl_PATH := vendor/trustonic/platform/$(shell echo $(MTK_PLATFORM) | tr '[A-Z]' '[a-z]')/external/mobicore/common/curl/include
#libgmp_PATH := external/strongswan-support/gmp
openssl_PATH := $(strongswan_PATH)/src/openssl/include

# some definitions
strongswan_DIR := "/vendor/bin"
strongswan_SBINDIR := "/vendor/bin"
strongswan_PIDDIR := "/data/misc/vpn"
strongswan_PLUGINDIR := "$(strongswan_IPSEC_DIR)/ipsec"
strongswan_CONFDIR := "/vendor/etc/ipsec"
strongswan_STRONGSWAN_CONF := "$(strongswan_CONFDIR)/strongswan.conf"
strongswan_SCRIPT := "/vendor/etc/ipsec_script"

# CFLAGS (partially from a configure run using droid-gcc)
strongswan_CFLAGS := \
	-Wno-format \
	-Wno-pointer-sign \
	-Wno-pointer-arith \
	-Wno-sign-compare \
	-Wno-strict-aliasing \
	-DHAVE___BOOL \
	-DHAVE_STDBOOL_H \
	-DHAVE_ALLOCA_H \
	-DHAVE_ALLOCA \
	-DHAVE_CLOCK_GETTIME \
	-DHAVE_PRCTL \
	-DHAVE_LINUX_UDP_H \
	-DHAVE_STRUCT_SADB_X_POLICY_SADB_X_POLICY_PRIORITY \
	-DHAVE_IPSEC_MODE_BEET \
	-DHAVE_IPSEC_DIR_FWD \
	-DHAVE_IN6_PKTINFO \
	-DOPENSSL_NO_ENGINE \
	-DCONFIG_H_INCLUDED \
	-DCAPABILITIES \
	-DCAPABILITIES_NATIVE \
	-DMONOLITHIC \
	-DUSE_IKEV1 \
	-DUSE_IKEV2 \
	-DUSE_BUILTIN_PRINTF \
	-DDEBUG \
	-DROUTING_TABLE=0 \
	-DROUTING_TABLE_PRIO=220 \
	-DVERSION=\"$(strongswan_VERSION)\" \
	-DPLUGINDIR=\"$(strongswan_PLUGINDIR)\" \
	-DIPSEC_DIR=\"$(strongswan_DIR)\" \
	-DIPSEC_PIDDIR=\"$(strongswan_PIDDIR)\" \
	-DIPSEC_CONFDIR=\"$(strongswan_CONFDIR)\" \
	-DSTRONGSWAN_CONF=\"$(strongswan_STRONGSWAN_CONF)\" \
	-DDEV_RANDOM=\"/dev/random\" \
	-DDEV_URANDOM=\"/dev/urandom\" \
	-DIPSEC_SCRIPT=\"$(strongswan_SCRIPT)\" \
	-DRESOLV_CONF=\"${strongswan_CONFDIR}/resolv.conf\"

#	-DHAVE_PTHREAD_COND_TIMEDWAIT_MONOTONIC \
# only for Android 2.0+
strongswan_CFLAGS += \
	-DHAVE_IN6ADDR_ANY

strongswan_BUILD := \
	charon \
	libcharon \
	libhydra \
	libstrongswan \
	libsimaka \
	openssl \
	curl

#	\
#	libtncif \
#	libtnccs \
#	libimcv

ifneq ($(strongswan_BUILD_STARTER),)
strongswan_BUILD += \
	starter \
	stroke \
	ipsec
endif

ifneq ($(strongswan_BUILD_SCEPCLIENT),)
strongswan_BUILD += \
	scepclient
endif
include $(addprefix $(LOCAL_PATH)/src/,$(addsuffix /Android.mk, \
		 $(strongswan_BUILD)))
		#$(sort $(strongswan_BUILD))))

include $(CLEAR_VARS)

$(shell mkdir -p $(TARGET_OUT_VENDOR)/etc/ipsec/ssl)
$(shell cp $(MY_LOCAL_PATH)/epdg_conf/ipsec.conf $(TARGET_OUT_VENDOR)/etc/ipsec)
$(shell cp $(MY_LOCAL_PATH)/epdg_conf/strongswan.conf $(TARGET_OUT_VENDOR)/etc/ipsec)
$(shell cp $(MY_LOCAL_PATH)/epdg_conf/updown_script $(TARGET_OUT_VENDOR)/etc/ipsec)
$(shell cp $(MY_LOCAL_PATH)/epdg_conf/openssl.cnf $(TARGET_OUT_VENDOR)/etc/ipsec/ssl)
$(shell cp $(MY_LOCAL_PATH)/epdg_conf/wod_cust.conf $(TARGET_OUT_VENDOR)/etc/ipsec)
$(shell cp $(MY_LOCAL_PATH)/epdg_conf/wod_optr.conf $(TARGET_OUT_VENDOR)/etc/ipsec)
$(shell mkdir -p $(TARGET_OUT_VENDOR)/etc/ipsec/ipsec.d/cacerts)
$(shell cp $(MY_LOCAL_PATH)/epdg_cert/* $(TARGET_OUT_VENDOR)/etc/ipsec/ipsec.d/cacerts)
