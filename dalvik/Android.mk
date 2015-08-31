# Copyright (C) 2006 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

subdirs := $(addprefix $(LOCAL_PATH)/,$(addsuffix /Android.mk, \
		libdex \
		vm \
		dexgen \
		dexlist \
		dexopt \
		dexdump \
		dx \
		tools \
		unit-tests \
	))

include $(subdirs)


.PHONY: dex dex-debug
ifeq ($(DONT_INSTALL_DEX_FILES),true)
dex:
	@echo "Forcing a remake with DONT_INSTALL_DEX_FILES=false"
	$(hide) $(MAKE) DONT_INSTALL_DEX_FILES=false
else
# DONT_INSTALL_DEX_FILES is already false, so a normal make takes care of it.
dex: $(DEFAULT_GOAL)
endif

d :=
ifneq ($(GENERATE_DEX_DEBUG),)
d := debug
endif
ifneq ($(DONT_INSTALL_DEX_FILES),true)
d := $(d)-install
endif
ifneq ($(d),debug-install)
# generate the debug .dex files, with a copy in ./dalvik/DEBUG-FILES.
# We need to rebuild the .dex files for the debug output to be generated.
# The "touch -c $(DX)" is a hack that we know will force
# a rebuild of the .dex files.  If $(DX) doesn't exist yet,
# we won't touch it (-c) and the normal build will create
# the .dex files naturally.
dex-debug:
	@echo "Forcing an app rebuild with GENERATE_DEX_DEBUG=true"
	@touch -c $(DX)
	$(hide) $(MAKE) DONT_INSTALL_DEX_FILES=false GENERATE_DEX_DEBUG=true
else
# GENERATE_DEX_DEBUG and DONT_INSTALL_DEX_FILES are already set properly,
# so a normal make takes care of it.
dex-debug: $(DEFAULT_GOAL)
endif
