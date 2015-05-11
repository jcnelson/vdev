CONF_ROOT := .
CONF_ABS_ROOT := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
PREFIX ?= 

BUILD_ROOT := $(CONF_ROOT)/build/

INITRAMFS_ROOT := $(CONF_ROOT)/build/initramfs/
VDEV_INITRAMFS := $(CONF_ROOT)/initramfs/

CONF_DIR := $(PREFIX)/etc/vdev
ACTIONS_DIR := $(CONF_DIR)/actions

ACTIONS := $(wildcard actions/*.act)
ACTIONS_BUILD := $(patsubst actions/%.act,$(BUILD_ROOT)/etc/vdev/actions/%.act,$(ACTIONS))
ACTIONS_INSTALL := $(patsubst actions/%.act,$(CONF_DIR)/actions/%.act,$(ACTIONS))

IFNAMES_BUILD := $(BUILD_ROOT)/etc/vdev/ifnames.conf
CONF_BUILD := $(BUILD_ROOT)/etc/vdev/vdevd.conf
INITSCRIPT_BUILD := $(BUILD_ROOT)/etc/init.d/vdev

VDEV_BUILD := $(CONF_BUILD) $(ACTIONS_BUILD) $(IFNAMES_BUILD) $(INITSCRIPT_BUILD)
VDEV_INSTALL := $(patsubst $(BUILD_ROOT)/%,$(PREFIX)/%,$(VDEV_BUILD))

TOOLS := ../tools/

GEN_IFNAMES := $(TOOLS)/persistent-net-names/gen-ifnames.sh
MKINITRAMFS := make-initramfs.sh

all: $(VDEV_BUILD)
print-%: ; @echo $*=$($*)
$(CONF_BUILD): $(CONF_ROOT)/vdevd.conf.in
	@mkdir -p $(shell dirname $@)
	@cat $< | \
		sed -e 's~@PREFIX@~$(PREFIX)~g;' | \
		sed -e 's~@CONF_DIR@~$(CONF_DIR)~g;' > $@

$(IFNAMES_BUILD): $(CONF_ROOT)/ifnames.conf.in 
	@mkdir -p $(shell dirname $@)
	@cat $< > $@
	$(GEN_IFNAMES) >> $@

$(INITSCRIPT_BUILD): $(CONF_ROOT)/sysv-initscript.sh 
	@mkdir -p $(shell dirname $@)
	@cat $< > $@

actions: $(ACTIONS_BUILD)
$(BUILD_ROOT)/etc/vdev/actions/%.act: actions/%.act
	@mkdir -p $(shell dirname $@)
	@cat $< > $@

install-files: $(VDEV_INSTALL)
$(PREFIX)/%: $(BUILD_ROOT)/%
	@mkdir -p $(shell dirname $@)
	@cat $< > $@

.PHONY: initramfs
initramfs: $(VDEV_INSTALL) 
	$(SHELL) ./$(MKINITRAMFS)

.PHONY: install
install: install-files
	# This is pretty ugly...what we really need is a "generic" device manager init script
	mkdir -p $(CONF_DIR)
	mkdir -p $(PREFIX)/etc/init.d/
	mkdir -p $(PREFIX)/etc/rcS.d/
	cp -a $(BUILD_ROOT)/vdev/* $(CONF_DIR)/
	cp -a $(INITSCRIPT) $(PREFIX)/etc/init.d/
	update-rc.d udev disable || true
	update-rc.d udev-finish disable || true
	[ -f $(PREFIX)/etc/init.d/.udev.disabled ] || mv $(PREFIX)/etc/init.d/udev $(PREFIX)/etc/init.d/.udev.disabled
	ln -s $(PREFIX)/etc/init.d/vdev $(PREFIX)/etc/rcS.d/S02vdev
	insserv

.PHONY: clean
clean:
	rm -rf $(CONF_ROOT)/build
