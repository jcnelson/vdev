# build environment
ROOT_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
BUILD    ?= $(ROOT_DIR)/build
OS ?= LINUX
BUILD_BINDIR := $(BUILD)/bin
BUILD_SBINDIR := $(BUILD)/sbin
BUILD_LIBDIR := $(BUILD)/lib
BUILD_USRBIN := $(BUILD)/usr/bin
BUILD_USRSHARE := $(BUILD)/usr/share
BUILD_USRSBIN := $(BUILD)/usr/sbin
BUILD_INCLUDEDIR := $(BUILD)/include/
BUILD_ETCDIR := $(BUILD)/etc

# install environment
DESTDIR			?= /
PREFIX         ?= /
BINDIR         ?= $(DESTDIR)$(PREFIX)/bin
SBINDIR			?= $(DESTDIR)$(PREFIX)/sbin
LIBDIR         ?= $(DESTDIR)$(PREFIX)/lib
INCLUDEDIR     ?= $(DESTDIR)/usr/include
USRBINDIR		?= $(DESTDIR)/usr/bin
USRSBINDIR		?= $(DESTDIR)/usr/sbin
USRSHAREDIR		?= $(DESTDIR)/usr/share
PKGCONFIGDIR   ?= $(DESTDIR)/usr/lib/pkgconfig
ETCDIR			?= $(DESTDIR)/etc

# libvdev 
BUILD_LIBVDEV := $(BUILD_LIBDIR)
BUILD_LIBVDEV_HEADERS := $(BUILD_INCLUDEDIR)/libvdev
BUILD_LIBVDEV_DIRS := $(BUILD_LIBVDEV) $(BUILD_LIBVDEV_HEADERS)
INSTALL_LIBVDEV := $(LIBDIR)

# vdevd 
BUILD_VDEVD := $(BUILD_SBINDIR)
BUILD_VDEVD_HELPERS := $(BUILD_LIBDIR)/vdev
BUILD_VDEVD_DIRS := $(BUILD_VDEVD) $(BUILD_VDEVD_HELPERS)
INSTALL_VDEVD := $(SBINDIR)
INSTALL_VDEVD_HELPERS := $(LIBDIR)/vdev

# vdevfs 
BUILD_VDEVFS := $(BUILD_USRSBIN)
BUILD_VDEVFS_DIRS := $(BUILD_VDEVFS)
INSTALL_VDEVFS := $(USRSBINDIR)

# config
BUILD_VDEV_CONFIG := $(BUILD_ETCDIR)/vdev
BUILD_VDEV_INITSCRIPT := $(BUILD_ETCDIR)/init.d
BUILD_VDEV_INITRAMFS := $(BUILD_USRSHARE)/initramfs-tools
BUILD_VDEV_CONFIG_DIRS := $(BUILD_VDEV_CONFIG)
INSTALL_VDEV_CONFIG := $(ETCDIR)/vdev
INSTALL_VDEV_INITSCRIPT := $(ETCDIR)/init.d
INSTALL_VDEV_INITRAMFS := $(USRSHAREDIR)/initramfs-tools

# compiler
CFLAGS     := -Wall -std=c99 -g -fPIC -fstack-protector -fstack-protector-all -pthread -Wno-unused-variable -Wno-unused-but-set-variable
CPPFLAGS   := -Wall -g -fPIC -fstack-protector -fstack-protector-all -pthread -Wno-unused-variable -Wno-unused-but-set-variable
INC      := -I. -I$(ROOT_DIR) -I$(BUILD_INCLUDEDIR)
DEFS     := -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_VDEV_OS_$(OS)
LIBINC   := -L$(BUILD_LIBVDEV)
CC       := gcc $(CFLAGS) $(INC) $(DEFS)
CPP      := g++ $(CPPFLAGS) $(INC) $(DEFS)

# build setup
BUILD_DIRS   := $(BUILD_VDEVD_DIRS) \
                $(BUILD_VDEVFS_DIRS) \
					 $(BUILD_LIBVDEV_DIRS)

all:

build_setup: $(BUILD_DIRS)

$(BUILD_DIRS):
	@mkdir -p $@

# debugging...
print-%: ; @echo $*=$($*)
