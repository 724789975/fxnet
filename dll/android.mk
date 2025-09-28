LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := fxnet_static
LOCAL_SRC_FILES := ../obj/local/$(TARGET_ARCH_ABI)/libfxnet_static.a
LOCAL_EXPORT_C_INCLUDES := ../include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := fxnet
LOCAL_SRC_FILES := $(foreach i, $(wildcard $(LOCAL_PATH)/src/*.cpp), $(patsubst $(LOCAL_PATH)/%, %, $i)) \
                  $(foreach i, $(wildcard $(LOCAL_PATH)/src/*.cc), $(patsubst $(LOCAL_PATH)/%, %, $i))

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include $(LOCAL_PATH)/include $(NDK_PATH)/sysroot/usr/include

LOCAL_STATIC_LIBRARIES := fxnet_static

# LOCAL_STATIC_LIBRARIES += libpthread
# LOCAL_LDFLAGS += $(SYSROOT)/usr/lib/$(TARGET_ARCH_ABI)/libpthread.a
LOCAL_LDLIBS := -Lcrypto -Lpthread

ifeq ($(SINGLE_THREAD),1)
    LOCAL_CFLAGS += -D__SINGLE_THREAD__
    LOCAL_CXXFLAGS += -D__SINGLE_THREAD__
endif

APP_STL := c++_static
include $(BUILD_SHARED_LIBRARY)