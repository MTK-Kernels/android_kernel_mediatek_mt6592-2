
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=\
    src/pppoe.c \
    src/if.c \
    src/debug.c \
    src/common.c \
    src/ppp.c \
    src/discovery.c

LOCAL_STATIC_LIBRARIES :=

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)

ifeq ($(HTC_PROJECT_FLAG),CT)
LOCAL_MODULE_TAGS := optional
else ifeq ($(HTCBCC_RIL_SKU),CT)
LOCAL_MODULE_TAGS := optional
else ifeq ($(COS_BUILD), true)
LOCAL_MODULE_TAGS := optional
else
LOCAL_MODULE_TAGS := debug
endif # HTC_PROJECT_FLAG

LOCAL_MODULE := pppoe
LOCAL_C_INCLUDES += $(LOCAL_PATH)/src
LOCAL_CFLAGS += '-DVERSION="3.10"' \
    '-DANDROID_CHANGES' \
    '-DPPPD_PATH="/system/bin/pppd"'

include $(BUILD_EXECUTABLE)

include $(call all-makefiles-under,$(LOCAL_PATH))
