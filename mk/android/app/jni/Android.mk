# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# libmain.so: the game, linked against the prebuilt SDL2.
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := main
LOCAL_SRC_FILES := $(patsubst $(LOCAL_PATH)/%,%,$(wildcard $(LOCAL_PATH)/*.c))
LOCAL_C_INCLUDES := $(LOCAL_PATH) $(LOCAL_PATH)/../SDL/include
# MOAS_VERSION comes from ndk-build (build-apk.sh passes MOAS_VERSION=...).
ifeq ($(MOAS_VERSION),)
  MOAS_VERSION := unknown
endif
LOCAL_CFLAGS += -std=c99 -Wall -O2 -fcommon -DLINUX -D_GNU_SOURCE \
	-DMOAS_DATADIR=\"\" -DMOAS_VERSION=\"$(MOAS_VERSION)\" -Dmain=SDL_main
LOCAL_SHARED_LIBRARIES := SDL2
LOCAL_LDLIBS := -llog -landroid -lm

include $(BUILD_SHARED_LIBRARY)
