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
BUILD_INCLUDEDIR := $(BUILD)/include
BUILD_ETCDIR := $(BUILD)/etc
BUILD_PKGCONFIG := $(BUILD)/usr/share/pkgconfig

# install environment
PREFIX         ?= /usr/local
INCLUDE_PREFIX ?= $(PREFIX)
BINDIR         ?= $(PREFIX)/bin
SBINDIR        ?= $(PREFIX)/sbin
LIBDIR         ?= $(PREFIX)/lib
INCLUDEDIR     ?= $(INCLUDE_PREFIX)/include
ETCDIR         ?= $(PREFIX)/etc
ETCDIR_VDEV    ?= $(ETCDIR)/vdev
SHAREDIR       ?= $(PREFIX)/share
PKGCONFIG	   ?= $(INCLUDE_PREFIX)/share/pkgconfig
RUNDIR         ?= $(PREFIX)/run
RUNDIR_VDEV    ?= $(RUNDIR)/vdev
LOGDIR         ?= $(PREFIX)/var/log
LOGDIR_VDEV    ?= $(LOGDIR)/vdev

# libvdev (NOTE: not an installable target; just common code)
LIBVDEV_ROOT := $(ROOT_DIR)/libvdev 
BUILD_LIBVDEV := $(BUILD_LIBDIR)
BUILD_LIBVDEV_HEADERS := $(BUILD_INCLUDEDIR)
BUILD_LIBVDEV_DIRS := $(BUILD_LIBVDEV) $(BUILD_LIBVDEV_HEADERS)

# libudev-compat 
BUILD_LIBUDEV_COMPAT := $(BUILD)/libudev-compat
BUILD_LIBUDEV_COMPAT_HEADERS := $(BUILD_INCLUDEDIR)
BUILD_LIBUDEV_COMPAT_DIRS := $(BUILD_LIBUDEV_COMPAT) $(BUILD_LIBUDEV_COMPAT_HEADERS)
INSTALL_LIBUDEV_COMPAT := $(DESTDIR)$(LIBDIR)
INSTALL_LIBUDEV_COMPAT_HEADERS := $(DESTDIR)$(INCLUDEDIR)
LIBUDEV_VERSION := 219

# vdevd 
BUILD_VDEVD := $(BUILD_SBINDIR)
BUILD_VDEVD_HELPERS := $(BUILD_LIBDIR)/vdev
BUILD_VDEVD_DIRS := $(BUILD_VDEVD) $(BUILD_VDEVD_HELPERS)
INSTALL_VDEVD := $(DESTDIR)$(SBINDIR)
INSTALL_VDEVD_HELPERS := $(DESTDIR)$(LIBDIR)/vdev

# vdevfs 
BUILD_VDEVFS := $(BUILD_USRSBIN)
BUILD_VDEVFS_DIRS := $(BUILD_VDEVFS)
INSTALL_VDEVFS := $(DESTDIR)$(SBINDIR)

# config
BUILD_VDEV_CONFIG := $(BUILD_ETCDIR)/vdev
BUILD_VDEV_INITSCRIPT := $(BUILD_ETCDIR)/init.d
BUILD_VDEV_INITRAMFS := $(BUILD_USRSHARE)/initramfs-tools
BUILD_VDEV_CONFIG_DIRS := $(BUILD_VDEV_CONFIG)
INSTALL_VDEV_CONFIG := $(DESTDIR)$(ETCDIR)/vdev
INSTALL_VDEV_INITSCRIPT := $(DESTDIR)$(ETCDIR)/init.d
INSTALL_VDEV_INITRAMFS := $(DESTDIR)$(SHAREDIR)/initramfs-tools

# hwdb 
BUILD_HWDB := $(BUILD_LIBDIR)/vdev/hwdb
BUILD_HWDB_DIRS := $(BUILD_HWDB)
INSTALL_HWDB := $(DESTDIR)$(LIBDIR)/vdev/hwdb

# compiler
CFLAGS     := -Wall -std=c99 -g -fPIC -fstack-protector -fstack-protector-all -pthread -Wno-unused-variable -Wno-unused-but-set-variable
LDFLAGS    :=
INC      := -I. -I$(ROOT_DIR) -I$(BUILD_INCLUDEDIR)
DEFS     := -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_VDEV_OS_$(OS) -D_XOPEN_SOURCE=700
LIBINC   := 
CC       ?= cc

# build setup
BUILD_DIRS   := $(sort $(BUILD_VDEVD_DIRS) \
                $(BUILD_VDEVFS_DIRS) \
					 $(BUILD_LIBUDEV_COMPAT_DIRS) \
					 $(BUILD_LIBVDEV_DIRS) \
					 $(BUILD_HWDB))

all:

build_setup: $(BUILD_DIRS)

$(BUILD_DIRS):
	@mkdir -p "$@"

# rule to make an archive member from an object file
(%.o): %.o ;    $(AR) cr $@ $*.o

# debugging...
print-%: ; @echo $*=$($*)
