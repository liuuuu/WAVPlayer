LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := WAVPlayer
LOCAL_SRC_FILES := WAVPlayer.cpp

# 使用静态库 WAVPlayer
LOCAL_STATIC_LIBRARIES += wavlib_static

# 与 OpenSL ES 链接
LOCAL_LDLIBS += -lOpenSLES

include $(BUILD_SHARED_LIBRARY)

# 引入WAVLib 库模块
$(call import-module, transcode-1.1.5/avilib)