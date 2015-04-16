CONF_ROOT := .
CONF_ABS_ROOT := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
PREFIX ?= 

BUILD_ROOT := $(CONF_ROOT)/build/etc/

INITRAMFS_ROOT := $(CONF_ROOT)/build/initramfs/
VDEV_INITRAMFS := $(CONF_ROOT)/initramfs/

CONF_DIR := $(PREFIX)/etc/vdev

IFNAMES := $(BUILD_ROOT)/vdev/ifnames.conf
CONF_GLOBAL := $(BUILD_ROOT)/vdev/vdevd.conf
INITSCRIPT := $(BUILD_ROOT)/init.d/vdev

TOOLS := ../tools/

GEN_IFNAMES := $(TOOLS)/persistent-net-names/gen-ifnames.sh
MKINITRAMFS := $(TOOLS)/initramfs/mkinitramfs

INITRAMFS_UDEV_FILES := $(INITRAMFS_ROOT)/scripts/init-top/udev \
								$(INITRAMFS_ROOT)/scripts/init-bottom/udev \
								$(INITRAMFS_ROOT)/hooks/udev \
								$(INITRAMFS_ROOT)/init

all: actions $(CONF_GLOBAL) $(IFNAMES) $(INITSCRIPT)

setup:
	mkdir -p $(BUILD_ROOT)
	mkdir -p $(BUILD_ROOT)/vdev
	mkdir -p $(BUILD_ROOT)/init.d
	mkdir -p $(CONF_ROOT)/build/initramfs/

$(CONF_GLOBAL): $(CONF_ROOT)/vdevd.conf.in setup
	@cat $(CONF_ROOT)/vdevd.conf.in | \
		sed -e 's~@PREFIX@~$(PREFIX)~g;' | \
		sed -e 's~@CONF_DIR@~$(CONF_DIR)~g;' > $(CONF_GLOBAL)

$(IFNAMES): $(CONF_ROOT)/ifnames.conf.in setup
	cp $(CONF_ROOT)/ifnames.conf.in $(IFNAMES)
	$(GEN_IFNAMES) >> $(IFNAMES)

$(INITSCRIPT): $(CONF_ROOT)/sysv-initscript.sh setup
	cp -a $(CONF_ROOT)/sysv-initscript.sh $(INITSCRIPT)

actions: setup
	cp -a $(CONF_ROOT)/actions $(BUILD_ROOT)/vdev/

initramfs: setup
	test -d /usr/share/initramfs-tools || (echo "You must install the initramfs-tools package" && exit 1)
	test -e /sbin/vdevd || (echo "You must install vdevd first" && exit 1)
	test -e /etc/vdev || (echo "You must install vdev's /etc files first" && exit 1)
	cp -a /usr/share/initramfs-tools/* $(INITRAMFS_ROOT)/
	rm $(INITRAMFS_UDEV_FILES)
	cp -a $(VDEV_INITRAMFS)/* $(INITRAMFS_ROOT)
	$(MKINITRAMFS) -t $(INITRAMFS_ROOT) -o $(CONF_ROOT)/initrd.img-`uname -r`

install:
	# This is pretty ugly...what we really need is a "generic" device manager init script
	mkdir -p $(CONF_DIR)
	mkdir -p $(PREFIX)/etc/init.d/
	mkdir -p $(PREFIX)/etc/rcS.d/
	cp -a $(BUILD_ROOT)/vdev/* $(CONF_DIR)/
	cp -a $(INITSCRIPT) $(PREFIX)/etc/init.d/
	ln -s $(PREFIX)/etc/init.d/vdev $(PREFIX)/etc/rcS.d/S02vdev

.PHONY: clean
clean:
	rm -rf $(CONF_ROOT)/build
