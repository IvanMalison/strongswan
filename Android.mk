LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# this is the list of plugins that are built into libstrongswan and charon
# also these plugins are loaded by default (if not changed in strongswan.conf)
strongswan_PLUGINS := aes des sha1 sha2 md5 fips-prf random x509 pubkey pkcs1 \
	pem xcbc hmac gmp kernel-netlink socket-default attr android

# helper macros to only add source files for plugins included in the list above
plugin_enabled = $(findstring $(1), $(strongswan_PLUGINS))
add_plugin = $(if $(call plugin_enabled,$(1)), \
				$(addprefix plugins/$(subst -,_,$(strip $(1))/),$(2)))

# includes
libvstr_PATH = external/strongswan-support/vstr/include
libgmp_PATH = external/strongswan-support/gmp

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
	-DHAVE_PTHREAD_COND_TIMEDWAIT_MONOTONIC \
	-DHAVE_PRCTL \
	-DHAVE_LINUX_UDP_H \
	-DHAVE_STRUCT_SADB_X_POLICY_SADB_X_POLICY_PRIORITY \
	-DHAVE_IPSEC_MODE_BEET \
	-DHAVE_IPSEC_DIR_FWD \
	-DMONOLITHIC \
	-DUSE_VSTR \
	-DROUTING_TABLE=0 \
	-DROUTING_TABLE_PRIO=220 \
	-DVERSION=\"4.4.0\" \
	-DPLUGINS='"$(strongswan_PLUGINS)"' \
	-DIPSEC_DIR=\"/system/bin\" \
	-DIPSEC_PIDDIR=\"/data/misc/vpn\" \
	-DSTRONGSWAN_CONF=\"/system/etc/strongswan.conf\" \
	-DDEV_RANDOM=\"/dev/random\" \
	-DDEV_URANDOM=\"/dev/urandom\"

# only for Android 2.0+
strongswan_CFLAGS += \
	-DHAVE_IN6ADDR_ANY

include $(addprefix $(LOCAL_PATH)/src/,$(addsuffix /Android.mk, \
		charon \
		libstrongswan \
	))
