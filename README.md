# WAVPlayer

修改 avilib/Android.mk 文件
```
LOCAL_PATH := $(call my-dir)

#
# 转码 WAVLib
#

# 源文件
MY_WAVLIB_SRC_FILES := wavlib.c platform_posix.c

# 包含导出目录
MY_WAVLIB_C_INCLUDES := $(LOCAL_PATH)

#
# WAVLib 静态
#
include $(CLEAR_VARS)

# 模块名称
LOCAL_MODULE := wavlib_static

# 源文件
LOCAL_SRC_FILES := $(MY_WAVLIB_SRC_FILES)

# 包含导出目录
LOCAL_EXPORT_C_INCLUDES := $(MY_WAVLIB_C_INCLUDES)

# 构建静态库
include $(BUILD_STATIC_LIBRARY)

#
# WAVLib 共享
#
include $(CLEAR_VARS)

# 模块名称
LOCAL_MODULE := wavlib_shared

# 源文件
LOCAL_SRC_FILES := $(MY_WAVLIB_SRC_FILES)

# 包含导出目录
LOCAL_EXPORT_C_INCLUDES := $(MY_WAVLIB_C_INCLUDES)

# 构建静态库
include $(BUILD_SHARED_LIBRARY)
```