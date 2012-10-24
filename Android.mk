LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := exynosv4l2_test.cpp

LOCAL_SHARED_LIBRARIES := libcutils
LOCAL_SHARED_LIBRARIES += libexynosv4l2
LOCAL_C_INCLUDES := \
	hardware/samsung_slsi/exynos/libexynosutils \
	hardware/samsung_slsi/exynos/include \
	hardware/samsung_slsi/exynos/libexynosutils

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := exynosv4l2_test

include $(BUILD_EXECUTABLE)
