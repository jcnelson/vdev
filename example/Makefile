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

INITSCRIPT_INSTALL := $(PREFIX)/etc/init.d/vdev

VDEV_BUILD := $(CONF_BUILD) $(ACTIONS_BUILD) $(IFNAMES_BUILD) $(INITSCRIPT_BUILD)
VDEV_INSTALL := $(patsubst $(BUILD_ROOT)/%,$(PREFIX)/%,$(VDEV_BUILD))

TOOLS := ../tools/

GEN_IFNAMES := $(TOOLS)/persistent-net-names/gen-ifnames.sh
MKINITRAMFS := make-initramfs.sh
VDEV_INSTALL_SCRIPT := vdev-install.sh

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

initscript_install: $(INITSCRIPT_INSTALL)
$(INITSCRIPT_INSTALL): $(INITSCRIPT_BUILD)
	@mkdir -p $(shell dirname $@)
	@cat $< > $@
	@chmod +x $@

actions: $(ACTIONS_BUILD)
$(BUILD_ROOT)/etc/vdev/actions/%.act: actions/%.act
	@mkdir -p $(shell dirname $@)
	@cat $< > $@

install-files: $(VDEV_INSTALL) initscript_install
$(PREFIX)/%: $(BUILD_ROOT)/%
	@mkdir -p $(shell dirname $@)
	@cat $< > $@

.PHONY: initramfs
initramfs: $(VDEV_INSTALL) 
	$(SHELL) ./$(MKINITRAMFS)

.PHONY: install
install: install-files
	$(SHELL) ./$(VDEV_INSTALL_SCRIPT) $(PREFIX)

.PHONY: clean
clean:
	@rm -rf $(CONF_ROOT)/build

.PHONY: uninstall
uninstall:
	@rm -f $(VDEV_INSTALL)
	update-rc.d -f vdev remove
	@echo "vdev configuration and init scripts have been removed."
	@echo "You will need to re-enable your previous device manager."
