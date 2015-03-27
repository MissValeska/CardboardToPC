LOCAL_PATH := $(call my-dir)

ARCH := $(APP_ABI)

include $(CLEAR_VARS)
 
LOCAL_LDLIBS := -llog
 
LOCAL_MODULE    := CardboardToPC
LOCAL_SRC_FILES := CardboardOnPC.c
 
include $(BUILD_SHARED_LIBRARY)
