# WickedEngine — build + test entry point for DMO (Editor + extension tests + upstream Tests binary).
#
# Submodule (fork): after `external/WickedEngine` points at https://github.com/AngryCloud/WickedEngine,
# clone with:  git submodule update --init --recursive
#
# Typical CI / local:
#   make -C external/WickedEngine configure build check
#
# Configure once, then iterate with `make build` / `make check`.
WICKED_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= build-dmo
CMAKE ?= cmake
CTEST ?= ctest
# macOS: sysctl; Linux: nproc
JOBS ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
GENERATOR ?= Ninja

CMAKE_CONFIGURE_FLAGS ?= \
	-DWICKED_EDITOR=ON \
	-DWICKED_TESTS=ON \
	-DBUILD_TESTING=ON

.PHONY: help configure build editor test-binaries check test ctest clean distclean

help:
	@echo "Targets:"
	@echo "  configure     CMake configure into $(BUILD_DIR) (Editor + DMO tests + upstream Tests)"
	@echo "  build         Build Editor, dmo_editor_extension_tests, and Tests"
	@echo "  check         Run CTest (dmo_editor_extension_tests only by default)"
	@echo "  test / ctest  Same as check"
	@echo "Variables: BUILD_DIR=$(BUILD_DIR) GENERATOR=$(GENERATOR)"

$(WICKED_ROOT)/$(BUILD_DIR)/CMakeCache.txt:
	"$(CMAKE)" -S "$(WICKED_ROOT)" -B "$(WICKED_ROOT)/$(BUILD_DIR)" \
		-G "$(GENERATOR)" $(CMAKE_CONFIGURE_FLAGS)

configure: $(WICKED_ROOT)/$(BUILD_DIR)/CMakeCache.txt

build: configure
	"$(CMAKE)" --build "$(WICKED_ROOT)/$(BUILD_DIR)" --parallel $(JOBS) \
		--target Editor dmo_editor_extension_tests Tests

editor: build

test-binaries: build

# Upstream Tests target is interactive/GPU; CTest only registers dmo_editor_extension_tests.
check ctest test: build
	cd "$(WICKED_ROOT)/$(BUILD_DIR)" && "$(CTEST)" --output-on-failure -R dmo_editor_extension_tests

clean:
	"$(CMAKE)" --build "$(WICKED_ROOT)/$(BUILD_DIR)" --target clean 2>/dev/null || true

distclean:
	rm -rf "$(WICKED_ROOT)/$(BUILD_DIR)"
