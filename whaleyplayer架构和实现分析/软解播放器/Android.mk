
LOCAL_PATH := $(call my-dir)

#
#Part 1-1: prebuild ffmpeg-0.8lib as shared library, then copy it to obj target
#

include $(CLEAR_VARS)

LOCAL_MODULE := libffmpeg

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE_SUFFIX := .so

LOCAL_MODULE_CLASS := SHARED_LIBRARIES

LOCAL_SRC_FILES_arm := libs/libffmpeg.so \

LOCAL_MODULE_TARGET_ARCHS := arm

LOCAL_MULTILIB := 32

include $(BUILD_PREBUILT)

#
#Part 1-1: prebuild ffmpeg-2.1 lib as shared library, then copy it to obj target
#

include $(CLEAR_VARS)

LOCAL_MODULE := libffmpeg2

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE_SUFFIX := .so

LOCAL_MODULE_CLASS := SHARED_LIBRARIES

LOCAL_SRC_FILES_arm := libs/libffmpeg2.so \

LOCAL_MODULE_TARGET_ARCHS := arm

LOCAL_MULTILIB := 32

include $(BUILD_PREBUILT)

#
#Part 2: compile WhaleyPlayer based on libffmpeg
#
include $(CLEAR_VARS)

FFMPEG_VERSION := 3.0

ifeq ($(FFMPEG_VERSION),3.0)
    LOCAL_CFLAGS := -DFFMPEG_30
    PATH_TO_FFMPEG_SOURCE := $(LOCAL_PATH)/ffmpeg-3.0
else
    PATH_TO_FFMPEG_SOURCE := $(LOCAL_PATH)/include
endif

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)  \
    $(PATH_TO_FFMPEG_SOURCE)  \
    external/icu/icu4c/source/common \

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libwhaleyplayer

LOCAL_CFLAGS += -D__STDC_CONSTANT_MACROS

LOCAL_SHARED_LIBRARIES := \
    libc \
    liblog \
    libdl \
    libcutils \
    libutils \
    libui \
    libmedia \
    libsurfaceflinger \
    libbinder \
    libz \
    libicuuc \
    libgui \
    libstagefright_foundation \

ifeq ($(FFMPEG_VERSION),3.0)
    LOCAL_SHARED_LIBRARIES += \
        libavcodec_whaley \
        libavutil_whaley \
        libavfilter_whaley \
        libswscale_whaley \
        libswresample_whaley \
        libavformat_whaley
else
    LOCAL_SHARED_LIBRARIES += libffmpeg
endif

LOCAL_MULTILIB := 32

LOCAL_SRC_FILES :=  \
    packetqueue.cpp \
    output.cpp \
    ffplayer.cpp \
    cueplayer.cpp \
    decoder.cpp \
    decoder_audio.cpp \
    thread.cpp   \
    cueparse.cpp \

ifeq ($(FFMPEG_VERSION),3.0)
    LOCAL_SRC_FILES +=  \
        decoder_video.cpp \
        framequeue.cpp \
        render_video.cpp \
        clock.cpp \
        decoder_subtitle.cpp
else

endif

LOCAL_SRC_FILES += \
    ff_func.c  \
    ff_decoder.c \
    ff_prt.c  \

LOCAL_SRC_FILES +=  \
    WhaleyPlayer.cpp \
    WhaleyMetadataRetriever.cpp \

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))


