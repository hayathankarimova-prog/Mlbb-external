LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := rsa

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

ALL_CPP := $(call rwildcard,$(LOCAL_PATH)/src/,*.cpp)
ALL_CPP := $(filter-out %CreateShaderModule.cpp,$(ALL_CPP))
ALL_C   := $(call rwildcard,$(LOCAL_PATH)/src/,*.c)

LOCAL_SRC_FILES := $(ALL_CPP:$(LOCAL_PATH)/%=%) $(ALL_C:$(LOCAL_PATH)/%=%)

LOCAL_C_INCLUDES := $(shell find $(LOCAL_PATH)/include $(LOCAL_PATH)/src -type d)

LOCAL_CPPFLAGS := -std=c++17 -O3 -fvisibility=hidden -Wno-error=format-security -Wno-format-security -DVK_USE_PLATFORM_ANDROID_KHR
LOCAL_LDLIBS := -llog -landroid -lGLESv2 -lGLESv3 -lEGL -lvulkan

include $(BUILD_EXECUTABLE)
