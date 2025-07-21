LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := libfxnet_static
LOCAL_SRC_FILES := $(foreach i, $(wildcard $(LOCAL_PATH)/*.cpp), $(patsubst $(LOCAL_PATH)/%, %, $i)) \
                  $(foreach i, $(wildcard $(LOCAL_PATH)/*.cc), $(patsubst $(LOCAL_PATH)/%, %, $i))

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include $(NDK_PATH)/sysroot/usr/include

ifeq ($(SINGLE_THREAD),1)
    LOCAL_CFLAGS += -D__SINGLE_THREAD__
    LOCAL_CXXFLAGS += -D__SINGLE_THREAD__
endif
include $(BUILD_STATIC_LIBRARY)