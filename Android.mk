ifneq ($(filter msm8960 msm8610,$(TARGET_BOARD_PLATFORM)),)
ifneq ($(BUILD_TINY_ANDROID),true)
LOCAL_PATH := $(call my-dir)

# HAL module implemenation stored in
include $(CLEAR_VARS)

ifneq ($(filter msm8610,$(TARGET_BOARD_PLATFORM)),)
  LOCAL_MODULE := sensors.$(TARGET_BOARD_PLATFORM)
  LOCAL_CFLAGS := -DTARGET_8610
else
  LOCAL_MODULE := sensors.msm8930
endif

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += -DLOG_TAG=\"Sensors\"
ifeq ($(call is-board-platform,msm8960),true)
  LOCAL_CFLAGS += -DTARGET_8930
endif

LOCAL_SRC_FILES :=	\
		sensors.cpp 			\
		SensorBase.cpp			\
		LightSensor.cpp			\
		ProximitySensor.cpp		\
		AkmSensor.cpp			\
		Accelerometer.cpp				\
		Mpu3050.cpp				\
		Bmp180.cpp				\
		InputEventReader.cpp

LOCAL_SHARED_LIBRARIES := liblog libcutils libdl

include $(BUILD_SHARED_LIBRARY)

endif #BUILD_TINY_ANDROID
endif #TARGET_BOARD_PLATFORM
