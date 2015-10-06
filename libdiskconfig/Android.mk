LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

commonSources := \
	diskconfig.c \
	diskutils.c \
	write_lst.c \
	config_mbr.c

include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(commonSources)
LOCAL_MODULE := libdiskconfig
LOCAL_MODULE_TAGS := optional
LOCAL_SYSTEM_SHARED_LIBRARIES := libcutils liblog libc
LOCAL_CFLAGS := -Werror
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(commonSources)
LOCAL_MODULE := libdiskconfig_host
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -O2 -g -W -Wall -Werror -D_LARGEFILE64_SOURCE -DHOST_BUILD
include $(BUILD_HOST_STATIC_LIBRARY)
