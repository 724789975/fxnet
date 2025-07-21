LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := libfxnet
LOCAL_SRC_FILES := $(foreach i, $(wildcard $(LOCAL_PATH)/*.cpp), $(patsubst $(LOCAL_PATH)/%, %, $i)) \
                  $(foreach i, $(wildcard $(LOCAL_PATH)/*.cc), $(patsubst $(LOCAL_PATH)/%, %, $i))

# �滻ԭ�е� LOCAL_CFLAGS/CXXFLAGS ����
LOCAL_CFLAGS += -D_POSIX_C_SOURCE=200809L
LOCAL_CXXFLAGS += -D_POSIX_C_SOURCE=200809L

# ������ǿ�ư��� pthread ͷ�ļ�·����������� NDK �汾��
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include $(NDK_PATH)/sysroot/usr/include

ifeq ($(SINGLE_THREAD),1)
    LOCAL_CFLAGS += -D__SINGLE_THREAD__
    LOCAL_CXXFLAGS += -D__SINGLE_THREAD__
endif
APP_STL := c++_shared
include $(BUILD_STATIC_LIBRARY)