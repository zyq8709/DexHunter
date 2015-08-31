#
# Copyright (C) 2011 The Android Open Source Project
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
#

LOCAL_PATH := $(call my-dir)

art_path := $(LOCAL_PATH)
art_build_path := $(art_path)/build

########################################################################
# clean-oat targets
#

# following the example of build's dont_bother for clean targets
ifneq (,$(filter clean-oat,$(MAKECMDGOALS)))
art_dont_bother := true
endif
ifneq (,$(filter clean-oat-host,$(MAKECMDGOALS)))
art_dont_bother := true
endif
ifneq (,$(filter clean-oat-target,$(MAKECMDGOALS)))
art_dont_bother := true
endif

.PHONY: clean-oat
clean-oat: clean-oat-host clean-oat-target

.PHONY: clean-oat-host
clean-oat-host:
	rm -f $(ART_NATIVETEST_OUT)/*.odex
	rm -f $(ART_NATIVETEST_OUT)/*.oat
	rm -f $(ART_NATIVETEST_OUT)/*.art
	rm -f $(ART_TEST_OUT)/*.odex
	rm -f $(ART_TEST_OUT)/*.oat
	rm -f $(ART_TEST_OUT)/*.art
	rm -f $(DALVIK_CACHE_OUT)/*@classes.dex
	rm -f $(DALVIK_CACHE_OUT)/*.oat
	rm -f $(DALVIK_CACHE_OUT)/*.art
	rm -f $(HOST_OUT_JAVA_LIBRARIES)/*.odex
	rm -f $(HOST_OUT_JAVA_LIBRARIES)/*.oat
	rm -f $(HOST_OUT_JAVA_LIBRARIES)/*.art
	rm -f $(TARGET_OUT_JAVA_LIBRARIES)/*.odex
	rm -f $(TARGET_OUT_JAVA_LIBRARIES)/*.oat
	rm -f $(TARGET_OUT_JAVA_LIBRARIES)/*.art
	rm -f $(TARGET_OUT_UNSTRIPPED)/system/framework/*.odex
	rm -f $(TARGET_OUT_UNSTRIPPED)/system/framework/*.oat
	rm -f $(TARGET_OUT_APPS)/*.odex
	rm -f $(TARGET_OUT_INTERMEDIATES)/JAVA_LIBRARIES/*_intermediates/javalib.odex
	rm -f $(TARGET_OUT_INTERMEDIATES)/APPS/*_intermediates/*.odex
	rm -rf /tmp/test-*/dalvik-cache/*@classes.dex

.PHONY: clean-oat-target
clean-oat-target:
	adb remount
	adb shell rm $(ART_NATIVETEST_DIR)/*.odex
	adb shell rm $(ART_NATIVETEST_DIR)/*.oat
	adb shell rm $(ART_NATIVETEST_DIR)/*.art
	adb shell rm $(ART_TEST_DIR)/*.odex
	adb shell rm $(ART_TEST_DIR)/*.oat
	adb shell rm $(ART_TEST_DIR)/*.art
	adb shell rm $(DALVIK_CACHE_DIR)/*.dex
	adb shell rm $(DALVIK_CACHE_DIR)/*.oat
	adb shell rm $(DALVIK_CACHE_DIR)/*.art
	adb shell rm $(DEXPREOPT_BOOT_JAR_DIR)/*.oat
	adb shell rm $(DEXPREOPT_BOOT_JAR_DIR)/*.art
	adb shell rm system/app/*.odex
	adb shell rm data/run-test/test-*/dalvik-cache/*@classes.dex

ifneq ($(art_dont_bother),true)

########################################################################
# product targets
include $(art_path)/runtime/Android.mk
include $(art_path)/compiler/Android.mk
include $(art_path)/dex2oat/Android.mk
include $(art_path)/oatdump/Android.mk
include $(art_path)/dalvikvm/Android.mk
include $(art_path)/jdwpspy/Android.mk
include $(art_build_path)/Android.oat.mk

# ART_HOST_DEPENDENCIES depends on Android.executable.mk above for ART_HOST_EXECUTABLES
ART_HOST_DEPENDENCIES := $(ART_HOST_EXECUTABLES) $(HOST_OUT_JAVA_LIBRARIES)/core-hostdex.jar
ART_HOST_DEPENDENCIES += $(HOST_OUT_SHARED_LIBRARIES)/libjavacore$(ART_HOST_SHLIB_EXTENSION)
ART_TARGET_DEPENDENCIES := $(ART_TARGET_EXECUTABLES) $(TARGET_OUT_JAVA_LIBRARIES)/core.jar $(TARGET_OUT_SHARED_LIBRARIES)/libjavacore.so

########################################################################
# test targets

include $(art_path)/test/Android.mk
include $(art_build_path)/Android.gtest.mk

# The ART_*_TEST_DEPENDENCIES definitions:
# - depend on Android.oattest.mk above for ART_TEST_*_DEX_FILES
# - depend on Android.gtest.mk above for ART_*_TEST_EXECUTABLES
ART_HOST_TEST_DEPENDENCIES   := $(ART_HOST_DEPENDENCIES)   $(ART_HOST_TEST_EXECUTABLES)   $(ART_TEST_HOST_DEX_FILES)   $(HOST_CORE_IMG_OUT)
ART_TARGET_TEST_DEPENDENCIES := $(ART_TARGET_DEPENDENCIES) $(ART_TARGET_TEST_EXECUTABLES) $(ART_TEST_TARGET_DEX_FILES) $(TARGET_CORE_IMG_OUT)

include $(art_build_path)/Android.libarttest.mk

# "mm test-art" to build and run all tests on host and device
.PHONY: test-art
test-art: test-art-host test-art-target
	@echo test-art PASSED

.PHONY: test-art-gtest
test-art-gtest: test-art-host-gtest test-art-target-gtest
	@echo test-art-gtest PASSED

.PHONY: test-art-oat
test-art-oat: test-art-host-oat test-art-target-oat
	@echo test-art-oat PASSED

.PHONY: test-art-run-test
test-art-run-test: test-art-host-run-test test-art-target-run-test
	@echo test-art-run-test PASSED

########################################################################
# host test targets

# "mm test-art-host" to build and run all host tests
.PHONY: test-art-host
test-art-host: test-art-host-gtest test-art-host-oat test-art-host-run-test
	@echo test-art-host PASSED

.PHONY: test-art-host-interpreter
test-art-host-interpreter: test-art-host-oat-interpreter test-art-host-run-test-interpreter
	@echo test-art-host-interpreter PASSED

.PHONY: test-art-host-dependencies
test-art-host-dependencies: $(ART_HOST_TEST_DEPENDENCIES) $(HOST_OUT_SHARED_LIBRARIES)/libarttest$(ART_HOST_SHLIB_EXTENSION) $(HOST_CORE_DEX_LOCATIONS)

.PHONY: test-art-host-gtest
test-art-host-gtest: $(ART_HOST_TEST_TARGETS)
	@echo test-art-host-gtest PASSED

define run-host-gtests-with
  $(foreach file,$(sort $(ART_HOST_TEST_EXECUTABLES)),$(1) $(file) &&) true
endef

# "mm valgrind-test-art-host-gtest" to build and run the host gtests under valgrind.
.PHONY: valgrind-test-art-host-gtest
valgrind-test-art-host-gtest: test-art-host-dependencies
	$(call run-host-gtests-with,valgrind --leak-check=full)
	@echo valgrind-test-art-host-gtest PASSED

.PHONY: test-art-host-oat-default
test-art-host-oat-default: $(ART_TEST_HOST_OAT_DEFAULT_TARGETS)
	@echo test-art-host-oat-default PASSED

.PHONY: test-art-host-oat-interpreter
test-art-host-oat-interpreter: $(ART_TEST_HOST_OAT_INTERPRETER_TARGETS)
	@echo test-art-host-oat-interpreter PASSED

.PHONY: test-art-host-oat
test-art-host-oat: test-art-host-oat-default test-art-host-oat-interpreter
	@echo test-art-host-oat PASSED

define declare-test-art-host-run-test
.PHONY: test-art-host-run-test-default-$(1)
test-art-host-run-test-default-$(1): test-art-host-dependencies
	art/test/run-test --host $(1)
	@echo test-art-host-run-test-default-$(1) PASSED

TEST_ART_HOST_RUN_TEST_DEFAULT_TARGETS += test-art-host-run-test-default-$(1)

.PHONY: test-art-host-run-test-interpreter-$(1)
test-art-host-run-test-interpreter-$(1): test-art-host-dependencies
	art/test/run-test --host --interpreter $(1)
	@echo test-art-host-run-test-interpreter-$(1) PASSED

TEST_ART_HOST_RUN_TEST_INTERPRETER_TARGETS += test-art-host-run-test-interpreter-$(1)

.PHONY: test-art-host-run-test-$(1)
test-art-host-run-test-$(1): test-art-host-run-test-default-$(1) test-art-host-run-test-interpreter-$(1)

endef

$(foreach test, $(wildcard art/test/[0-9]*), $(eval $(call declare-test-art-host-run-test,$(notdir $(test)))))

.PHONY: test-art-host-run-test-default
test-art-host-run-test-default: $(TEST_ART_HOST_RUN_TEST_DEFAULT_TARGETS)
	@echo test-art-host-run-test-default PASSED

.PHONY: test-art-host-run-test-interpreter
test-art-host-run-test-interpreter: $(TEST_ART_HOST_RUN_TEST_INTERPRETER_TARGETS)
	@echo test-art-host-run-test-interpreter PASSED

.PHONY: test-art-host-run-test
test-art-host-run-test: test-art-host-run-test-default test-art-host-run-test-interpreter
	@echo test-art-host-run-test PASSED

########################################################################
# target test targets

# "mm test-art-target" to build and run all target tests
.PHONY: test-art-target
test-art-target: test-art-target-gtest test-art-target-oat test-art-target-run-test
	@echo test-art-target PASSED

.PHONY: test-art-target-dependencies
test-art-target-dependencies: $(ART_TARGET_TEST_DEPENDENCIES) $(ART_TEST_OUT)/libarttest.so

.PHONY: test-art-target-sync
test-art-target-sync: test-art-target-dependencies
	adb remount
	adb sync
	adb shell mkdir -p $(ART_TEST_DIR)

.PHONY: test-art-target-gtest
test-art-target-gtest: $(ART_TARGET_TEST_TARGETS)

.PHONY: test-art-target-oat
test-art-target-oat: $(ART_TEST_TARGET_OAT_TARGETS)
	@echo test-art-target-oat PASSED

define declare-test-art-target-run-test
.PHONY: test-art-target-run-test-$(1)
test-art-target-run-test-$(1): test-art-target-sync
	art/test/run-test $(1)
	@echo test-art-target-run-test-$(1) PASSED

TEST_ART_TARGET_RUN_TEST_TARGETS += test-art-target-run-test-$(1)

test-art-run-test-$(1): test-art-host-run-test-$(1) test-art-target-run-test-$(1)

endef

$(foreach test, $(wildcard art/test/[0-9]*), $(eval $(call declare-test-art-target-run-test,$(notdir $(test)))))

.PHONY: test-art-target-run-test
test-art-target-run-test: $(TEST_ART_TARGET_RUN_TEST_TARGETS)
	@echo test-art-target-run-test PASSED

########################################################################
# oat-target and oat-target-sync targets

OAT_TARGET_TARGETS :=

# $(1): input jar or apk target location
define declare-oat-target-target
ifneq (,$(filter $(1),$(addprefix system/app/,$(addsuffix .apk,$(PRODUCT_DEX_PREOPT_PACKAGES_IN_DATA)))))
OUT_OAT_FILE := $(call dalvik-cache-out,$(1)/classes.dex)
else
OUT_OAT_FILE := $(PRODUCT_OUT)/$(basename $(1)).odex
endif

ifeq ($(ONE_SHOT_MAKEFILE),)
# ONE_SHOT_MAKEFILE is empty for a top level build and we don't want
# to define the oat-target-* rules there because they will conflict
# with the build/core/dex_preopt.mk defined rules.
.PHONY: oat-target-$(1)
oat-target-$(1):

else
.PHONY: oat-target-$(1)
oat-target-$(1): $$(OUT_OAT_FILE)

$$(OUT_OAT_FILE): $(PRODUCT_OUT)/$(1) $(TARGET_BOOT_IMG_OUT) $(DEX2OAT_DEPENDENCY)
	@mkdir -p $$(dir $$@)
	$(DEX2OAT) $(PARALLEL_ART_COMPILE_JOBS) --runtime-arg -Xms64m --runtime-arg -Xmx64m --boot-image=$(TARGET_BOOT_IMG_OUT) --dex-file=$(PRODUCT_OUT)/$(1) --dex-location=/$(1) --oat-file=$$@ --host-prefix=$(PRODUCT_OUT) --instruction-set=$(TARGET_ARCH) --android-root=$(PRODUCT_OUT)/system

endif

OAT_TARGET_TARGETS += oat-target-$(1)
endef

$(foreach file,\
  $(filter-out\
    $(addprefix $(TARGET_OUT_JAVA_LIBRARIES)/,$(addsuffix .jar,$(TARGET_BOOT_JARS))),\
    $(wildcard $(TARGET_OUT_APPS)/*.apk) $(wildcard $(TARGET_OUT_JAVA_LIBRARIES)/*.jar)),\
  $(eval $(call declare-oat-target-target,$(subst $(PRODUCT_OUT)/,,$(file)))))

.PHONY: oat-target
oat-target: $(ART_TARGET_DEPENDENCIES) $(TARGET_BOOT_OAT_OUT) $(OAT_TARGET_TARGETS)

.PHONY: oat-target-sync
oat-target-sync: oat-target
	adb remount
	adb sync

########################################################################
# "m build-art" for quick minimal build
.PHONY: build-art
build-art: build-art-host build-art-target

.PHONY: build-art-host
build-art-host:   $(ART_HOST_EXECUTABLES)   $(ART_HOST_TEST_EXECUTABLES)   $(HOST_CORE_IMG_OUT)   $(HOST_OUT)/lib/libjavacore.so

.PHONY: build-art-target
build-art-target: $(ART_TARGET_EXECUTABLES) $(ART_TARGET_TEST_EXECUTABLES) $(TARGET_CORE_IMG_OUT) $(TARGET_OUT)/lib/libjavacore.so

########################################################################
# oatdump targets

.PHONY: dump-oat
dump-oat: dump-oat-core dump-oat-boot

.PHONY: dump-oat-core
dump-oat-core: dump-oat-core-host dump-oat-core-target

.PHONY: dump-oat-core-host
ifeq ($(ART_BUILD_HOST),true)
dump-oat-core-host: $(HOST_CORE_IMG_OUT) $(OATDUMP)
	$(OATDUMP) --image=$(HOST_CORE_IMG_OUT) --output=/tmp/core.host.oatdump.txt --host-prefix=""
	@echo Output in /tmp/core.host.oatdump.txt
endif

.PHONY: dump-oat-core-target
ifeq ($(ART_BUILD_TARGET),true)
dump-oat-core-target: $(TARGET_CORE_IMG_OUT) $(OATDUMP)
	$(OATDUMP) --image=$(TARGET_CORE_IMG_OUT) --output=/tmp/core.target.oatdump.txt
	@echo Output in /tmp/core.target.oatdump.txt
endif

.PHONY: dump-oat-boot
ifeq ($(ART_BUILD_TARGET_NDEBUG),true)
dump-oat-boot: $(TARGET_BOOT_IMG_OUT) $(OATDUMP)
	$(OATDUMP) --image=$(TARGET_BOOT_IMG_OUT) --output=/tmp/boot.oatdump.txt
	@echo Output in /tmp/boot.oatdump.txt
endif

.PHONY: dump-oat-Calculator
ifeq ($(ART_BUILD_TARGET_NDEBUG),true)
dump-oat-Calculator: $(TARGET_OUT_APPS)/Calculator.odex $(TARGET_BOOT_IMG_OUT) $(OATDUMP)
	$(OATDUMP) --oat-file=$< --output=/tmp/Calculator.oatdump.txt
	@echo Output in /tmp/Calculator.oatdump.txt
endif

########################################################################
# cpplint targets to style check art source files

include $(art_build_path)/Android.cpplint.mk

########################################################################
# targets to switch back and forth from libdvm to libart

.PHONY: use-art
use-art:
	adb root && sleep 3
	adb shell setprop persist.sys.dalvik.vm.lib libart.so
	adb reboot

.PHONY: use-artd
use-artd:
	adb root && sleep 3
	adb shell setprop persist.sys.dalvik.vm.lib libartd.so
	adb reboot

.PHONY: use-dalvik
use-dalvik:
	adb root && sleep 3
	adb shell setprop persist.sys.dalvik.vm.lib libdvm.so
	adb reboot

########################################################################

endif # !art_dont_bother
