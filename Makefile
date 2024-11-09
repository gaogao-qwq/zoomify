.PHONY: all clean

PLATFORM   ?= PLATFORM_DESKTOP
BUILD_MODE ?= DEBUG
OS         ?= Unknown
THIS_FILE  ?= $(lastword $(MAKEFILE_LIST))

ifeq ($(OS),Windows_NT)
	OS = Windows
else
	UNAME := $(shell uname -s)
	ifeq ($(UNAME),Linux)
		OS = Linux
	endif
	ifeq ($(UNAME),Darwin)
		OS = macOS
	endif
endif

ifeq ($(OS),Linux)
	CC = gcc
endif
ifeq ($(OS),macOS)
	CC = clang
endif

ifeq ($(BUILD_MODE),DEBUG)
	DEBUG_FLAG := -g -DDEBUG
endif

ifeq ($(BUILD_MODE),RELEASE)
	EXTRA_FLAG := -O3
endif

all: magnifier

magnifier:
	@mkdir -p build
ifeq ($(OS),Linux)
	@$(MAKE) -f $(THIS_FILE) linux_build
endif
ifeq ($(OS),macOS)
	@$(MAKE) -f $(THIS_FILE) macos_build
endif

windows_build:
	@echo 'TODO: implement Windows build'

linux_build:
	$(CC) -o build/magnifier \
		-I include -L lib -lm \
		-Wall -Wextra $(DEBUG_FLAG) $(EXTRA_FLAG) \
		src/magnifier.c lib/libraylib.a

macos_build:
ifeq ($(BUILD_MODE),DEBUG)
	xcodebuild -configuration Debug -scheme magnifier \
		-destination 'platform=macOS,arch=arm64' SYMROOT="build/Debug" DSTROOT="build/Release"
endif
ifeq ($(BUILD_MODE),RELEASE)
	xcodebuild -configuration Release -scheme magnifier \
		-destination 'platform=macOS,arch=arm64' SYMROOT="build/Debug" DSTROOT="build/Release"
endif

clean:
	echo "cleaning build..."
	rm -r build/*
