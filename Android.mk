LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := monitor.c
LOCAL_MODULE := monitor
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := inf-loop.c
LOCAL_MODULE := inf-loop
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := cpu-stats.c
LOCAL_MODULE := cpu-stats
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
