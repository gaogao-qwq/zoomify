.PHONY: all zoomify configure clean install uninstall

PLATFORM   ?= PLATFORM_DESKTOP
BUILD_MODE ?= DEBUG
OS         ?= UNKNOWN
THIS_FILE  ?= $(lastword $(MAKEFILE_LIST))

LIBRAYLIB_PATH ?= lib/libraylib.a

ifeq ($(OS),Windows_NT)
	OS = WINDOWS
else
	UNAME := $(shell uname -s)
	ifeq ($(UNAME),Linux)
		OS = LINUX
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
	DEBUG_FLAG := -g -DDEBUG
endif

ifeq ($(BUILD_MODE),RELEASE)
	EXTRA_FLAG := -O3 -DRELEASE
endif

all: zoomify

configure:
	git submodule update --init --depth=1
	cd raylib/src && $(MAKE) PLATFORM=PLATFORM_DESKTOP
	@mkdir -p lib
	cp raylib/src/libraylib.a $(LIBRAYLIB_PATH)

zoomify:
	@if test ! -f "lib/libraylib.a"; then \
		echo "WARNING: $(LIBRAYLIB_PATH) not found, running 'make configure'..."; \
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
	$(CC) -o build/zoomify \
		-I include -L lib -lm -lX11 \
		-Wall -Wextra $(DEBUG_FLAG) $(EXTRA_FLAG) \
		src/zoomify.c src/linux_screenshot.c lib/libraylib.a

macos_build:
ifeq ($(BUILD_MODE),DEBUG)
	xcodebuild -configuration Debug -scheme zoomify \
		-destination 'platform=macOS,arch=arm64' SYMROOT="build" DSTROOT="build"
endif
ifeq ($(BUILD_MODE),RELEASE)
	xcodebuild -configuration Release -scheme zoomify \
		-destination 'platform=macOS,arch=arm64' SYMROOT="build" DSTROOT="build"
endif

install:
	BUILD_MODE=RELEASE $(MAKE) -f $(THIS_FILE) zoomify
ifeq ($(OS),MACOS)
	sudo cp build/Release/zoomify /usr/local/bin/zoomify
endif
ifeq ($(OS),LINUX)
	sudo cp build/zoomify /usr/local/bin/zoomify
endif

uninstall:
ifeq ($(OS),MACOS)
	rm /usr/local/bin/zoomify
endif
ifeq ($(OS),LINUX)
	rm /usr/local/bin/zoomify
endif

clean:
	echo "cleaning build..."
	rm -r build/*
