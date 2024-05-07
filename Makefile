
# Default flags
MAKEFLAGS := -r -R
MAKEFLAGS += -s

# Use one shell to process entire recipe
.ONESHELL:

MAKEFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
CURR_DIR := $(notdir $(patsubst %/,%,$(dir $(MAKEFILE_PATH))))
ROOT_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))


PROJECT   ?= bombus
TARGET    ?= $(shell uname -m)
# COVERAGE  ?=
# SANITIZE  ?=
# ANALYZE   ?=

EXTLIB_DIR   ?= /opt/sdk/$(PROJECT)/$(TARGET)
INSTALL_DIR  ?= /opt/sdk/$(PROJECT)/$(TARGET)
SANOUT_DIR   ?= /tmp/$(PROJECT)/$(TARGET)

BUILD_T_DIR  := $(ROOT_DIR)/build_$(TARGET)


CMAKE_TARGET_OPTIONS =
CMAKE_CFLAGS_OPTIONS =
CMAKE_CXXFLAGS_OPTIONS =
CMAKE_LDFLAGS_OPTIONS =



MAKE_USER_MODULES ?= config

include $(MAKE_USER_MODULES)/target.$(TARGET).mk
include $(MAKE_USER_MODULES)/tools.mk





all: debug \
     release

cmake: cmake_debug \
       cmake_release

test: test_unit

report: report_test_unit

debug:     build_debug
release:   build_release
test_unit: build_test_unit run_test_unit report_test_unit
test_fat:  build_test_fat  run_test_fat report_test_fat

sanitize: build_sanitize
coverage: build_coverage
analyze:  build_analyze





install_%: build_%
	$(eval VARIANT := $(subst install_,,$@))
	cd build_$(TARGET)/$(VARIANT)
	make install



build_%: cmake_%
	$(eval VARIANT := $(subst build_,,$@))
	cd build_$(TARGET)/$(VARIANT)

ifeq ($(ANALYZE),1)
	export CCC_CC=clang
	export CCC_CXX=clang++

	scan-build -analyze-headers -o $(BUILD_T_DIR)/report_analyze make
else
	make
endif



cmake_%: dir_%
	$(eval VARIANT := $(subst cmake_,,$@))
	cd build_$(TARGET)/$(VARIANT)

	$(eval VARIANT := $(if $(findstring test_unit,$(VARIANT)),test,$(VARIANT)))

ifneq ($(CMAKE_CFLAGS_OPTIONS),)
	export CFLAGS="$(CMAKE_CFLAGS_OPTIONS)"
endif
ifneq ($(CMAKE_CXXFLAGS_OPTIONS),)
	export CXXFLAGS="$(CMAKE_CXXFLAGS_OPTIONS)"
endif
ifneq ($(CMAKE_LDFLAGS_OPTIONS),)
	export LDFLAGS="$(CMAKE_LDFLAGS_OPTIONS)"
endif

ifeq ($(SANITIZE),1)
	export ASAN_OPTIONS="detect_leaks=1:log_path=$(SANOUT_DIR)/asan"
	export TSAN_OPTIONS="log_path=/$(SANOUT_DIR)/tsan"
	export UBSAN_OPTIONS="log_path=/$(SANOUT_DIR)/ubsan"
endif

	cmake \
		-DCMAKE_PROJECT_NAME=$(PROJECT) \
		-DCMAKE_BUILD_TARGET=$(TARGET) \
		-DCMAKE_BUILD_VARIANT=$(VARIANT) \
		-DCMAKE_FIND_ROOT_PATH="$(EXTLIB_DIR)" \
		-DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
		$(CMAKE_TARGET_OPTIONS) \
		../..



dir_%:
	$(eval VARIANT := $(subst dir_,,$@))
	echo ""
	echo "    Build $(TARGET) $(VARIANT)"
	echo ""
	mkdir -p build_$(TARGET)/$(VARIANT)



run_%:
	$(eval VARIANT := $(subst run_,,$@))
	test/run_$(VARIANT) "$(PROJECT)_test" "$(BUILD_T_DIR)/$(VARIANT)" "$(ROOT_DIR)"
	echo ""



report_%:
	$(eval VARIANT := $(subst report_,,$@))
	$(eval REPORT_DIR := "$(BUILD_T_DIR)/report_coverage")
	mkdir -p "$(REPORT_DIR)"
	rm -f $$(find "$(BUILD_T_DIR)/$(VARIANT)" -name "*.gcda" | grep "test/cunit")
	lcov --base-directory "$(BUILD_T_DIR)/$(VARIANT)" --directory "." --no-external --capture --output "$(REPORT_DIR)/$(PROJECT)_$(VARIANT).inf"
	INF_FILES=$$(find "$(REPORT_DIR)" -name "*.inf")
	genhtml --output-directory "$(REPORT_DIR)" $$INF_FILES



doxygen: cmake_debug
	cd build_$(TARGET)/debug
	make doc
	rm -rf ../doc
	mv doc ..



format:
	CHECK_FILES=$$(find . -name "*.[ch]")
	uncrustify -c $(ROOT_DIR)/config/uncrustify.cfg --no-backup $$CHECK_FILES



clean:
	rm -rf build*



prepare_test:
	mkdir -p /tmp/$(PROJECT)
	cp -r $(ROOT_DIR)/test/cert /tmp/$(PROJECT)



help:
	echo ""
	echo "Usage:   make [options] <variant>"
	echo ""
	echo "OPTIONS:"
	echo "TARGET=<target>  - specify target"
	echo "COVERAGE=1       - enable gcov"
	echo "SANITIZE=1       - enable clang sanitizer"
	echo "ANALYZE=1        - enable clang analyzer"
	echo ""
	echo "EXAMPLES:"
	echo "make release"
	echo "make COVERAGE=1 test"
	echo "make TARGET=x86_64 ANALYZE=1 debug"
	echo ""
	echo "OTHER:"
	echo "make deps        - install dependencies"
	echo ""
	echo "make build_test_unit  - build unit test app"
	echo "make build_test_fat   - build fat test app"
	echo "make run_test_unit    - run unit test app"
	echo "make run_test_fat     - run fat test app"
	echo "make report_test_unit - generate unit coverage report"
	echo "make report_test_fat  - generate fat coverage report"
	echo ""



deps:
	sudo apt update
	sudo apt install clang clang-tools lcov uncrustify
	sudo apt install libcunit1-dev
	sudo apt install libjson-c-dev

