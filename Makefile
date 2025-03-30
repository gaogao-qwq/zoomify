.PHONY: all zoomify configure parse_xcode_build_log clean install uninstall

PLATFORM         ?= PLATFORM_DESKTOP
BUILD_MODE       ?= DEBUG
OS               ?= UNKNOWN
DISPLAY_PROTOCOL ?= ALL
THIS_FILE        ?= $(lastword $(MAKEFILE_LIST))

LIBRAYLIB_PATH      ?= lib/libraylib.a
SHADER_HEADERS_PATH ?= include/shaders.h
ZOOMIFY_XCWORKSPACE_PATH = zoomify.xcodeproj/project.xcworkspace
ZOOMIFYD_XCWORKSPACE_PATH = zoomifyd/zoomifyd.xcodeproj/project.xcworkspace
ZOOMIFYD_BUILD_LOG = build/zoomifyd_build.log

ifeq ($(OS),Windows_NT)
	OS = WINDOWS
else
	UNAME := $(shell uname -s)
	ifeq ($(UNAME),Linux)
		OS = LINUX
		ifeq ($(DISPLAY_PROTOCOL),ALL)
			COMPILE_FLAG += $(shell pkg-config --cflags --libs x11 xinerama dbus-1 wayland-client) -DX11 -DWAYLAND
		endif
		ifeq ($(DISPLAY_PROTOCOL),X11)
			COMPILE_FLAG += $(shell pkg-config --cflags --libs x11 xinerama) -DX11
		endif
		ifeq ($(DISPLAY_PROTOCOL),WAYLAND)
			COMPILE_FLAG += $(shell pkg-config --cflags --libs dbus-1 wayland-client) -DWAYLAND
		endif
	endif
	ifeq ($(UNAME),Darwin)
		OS = MACOS
	endif
endif

ifeq ($(OS),LINUX)
	CC = gcc
endif
ifeq ($(OS),MACOS)
	CC = clang
endif

ifeq ($(BUILD_MODE),DEBUG)
	COMPILE_FLAG += -g -DDEBUG
endif

ifeq ($(BUILD_MODE),RELEASE)
	COMPILE_FLAG += -O3 -DRELEASE
endif

all: zoomify

configure:
	git submodule update --init --depth=1
	cd raylib/src && $(MAKE) clean
	cd raylib/src && $(MAKE) MACOSX_DEPLOYMENT_TARGET=10.9 PLATFORM=PLATFORM_DESKTOP
	@mkdir -p lib
	cp raylib/src/libraylib.a $(LIBRAYLIB_PATH)
	gcc -o build/generate_shader_header -O3 generate_shader_header.c
	build/generate_shader_header

zoomify:
	@if test ! -f $(LIBRAYLIB_PATH); then \
		echo "WARNING: $(LIBRAYLIB_PATH) not found, running 'make configure'..."; \
		$(MAKE) -f $(THIS_FILE) configure; \
	fi
	@if test ! -f $(SHADER_HEADERS_PATH); then \
		echo "WARNING: $(SHADER_HEADERS_PATH) not found, running 'make configure'..."; \
		$(MAKE) -f $(THIS_FILE) configure; \
	fi
	@mkdir -p build
ifeq ($(OS),LINUX)
	@$(MAKE) -f $(THIS_FILE) linux_build
endif
ifeq ($(OS),MACOS)
	@$(MAKE) -f $(THIS_FILE) macos_build
endif

windows_build:
	@echo 'TODO: implement Windows build'

linux_build:
	build/generate_shader_header
	$(CC) -o build/zoomify \
		-I include -L lib -lm \
		-Wall -Wextra $(COMPILE_FLAG) \
		src/zoomify.c src/linux_screenshot.c lib/libraylib.a

macos_build:
	build/generate_shader_header
ifeq ($(BUILD_MODE),DEBUG)
	# build zoomify
	xcodebuild -workspace $(ZOOMIFY_XCWORKSPACE_PATH) \
		-configuration Debug -scheme zoomify \
		-destination 'platform=macOS,arch=arm64' SYMROOT="build" DSTROOT="build"
	# build zoomifyd
	cp build/Debug/zoomify zoomifyd/zoomifyd
	xcodebuild -workspace $(ZOOMIFYD_XCWORKSPACE_PATH) \
		-scheme zoomifyd -configuration Debug \
		archive -archivePath build/Debug/zoomifyd > $(ZOOMIFYD_BUILD_LOG)
	rm zoomifyd/zoomifyd/zoomify
endif
ifeq ($(BUILD_MODE),RELEASE)
	# build zoomify
	xcodebuild -workspace $(ZOOMIFY_XCWORKSPACE_PATH) \
		-configuration Release -scheme zoomify \
		-destination 'platform=macOS,arch=arm64' DSTROOT="build"
	# build zoomifyd
	cp build/Release/zoomify zoomifyd/zoomifyd
	xcodebuild -workspace $(ZOOMIFYD_XCWORKSPACE_PATH) \
		-scheme zoomifyd -configuration Release \
		archive -archivePath build/Release/zoomifyd > $(ZOOMIFYD_BUILD_LOG)
	rm zoomifyd/zoomifyd/zoomify
endif

# parse xcodebuild build log to generate buildServer.json for sourcekit-lsp
# require xcode-build-server cli
parse_xcode_build_log:
	@if test ! -f $(ZOOMIFYD_BUILD_LOG); then \
		@echo "WARNING: $(ZOOMIFYD_BUILD_LOG) not found, running 'make macos_build' ..."; \
		$(MAKE) -f $(THIS_FILE) macos_build; \
	fi
	xcode-build-server parse -a $(ZOOMIFYD_BUILD_LOG)

install:
	@$(MAKE) -f $(THIS_FILE) zoomify BUILD_MODE=RELEASE
ifeq ($(OS),MACOS)
	cp -r build/Release/zoomifyd.xcarchive/Products/Applications/zoomifyd.app /Applications
endif
ifeq ($(OS),LINUX)
	sudo cp build/zoomify /usr/local/bin/zoomify
endif

uninstall:
ifeq ($(OS),MACOS)
	rm -r /Applications/zoomifyd.app
endif
ifeq ($(OS),LINUX)
	sudo rm /usr/local/bin/zoomify
endif

clean:
	echo "cleaning build..."
	rm -r build/*
