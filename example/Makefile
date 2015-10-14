include ../buildconf.mk

ACTIONS := $(wildcard actions/*.act)

ACTIONS_BUILD := $(patsubst actions/%.act,$(BUILD_VDEV_CONFIG)/actions/%.act,$(ACTIONS))
ACTIONS_INSTALL := $(patsubst actions/%.act,$(INSTALL_VDEV_CONFIG)/actions/%.act,$(ACTIONS))

IFNAMES_BUILD := $(BUILD_VDEV_CONFIG)/ifnames.conf
IFNAMES_INSTALL := $(INSTALL_VDEV_CONFIG)/ifnames.conf

CONF_BUILD := $(BUILD_VDEV_CONFIG)/vdevd.conf
CONF_INSTALL := $(INSTALL_VDEV_CONFIG)/vdevd.conf

INITSCRIPT_BUILD := $(BUILD_VDEV_INITSCRIPT)/vdev
INITSCRIPT_INSTALL := $(INSTALL_VDEV_INITSCRIPT)/vdev

INITRAMFS_FILES := $(wildcard initramfs/hooks/*) \
	                $(wildcard initramfs/scripts/init-bottom/*) \
	                $(wildcard initramfs/scripts/init-top/*) \
						 $(wildcard initramfs/init)

INITRAMFS_BUILD := $(patsubst initramfs/%,$(BUILD_VDEV_INITRAMFS)/%,$(INITRAMFS_FILES))
INITRAMFS_INSTALL := $(patsubst initramfs/%,$(INSTALL_VDEV_INITRAMFS)/%,$(INITRAMFS_FILES))

VDEV_CONFIG_BUILD := $(CONF_BUILD) $(ACTIONS_BUILD) $(IFNAMES_BUILD) $(INITSCRIPT_BUILD) $(INITRAMFS_BUILD) 
VDEV_CONFIG_INSTALL := $(CONF_INSTALL) $(ACTIONS_INSTALL) $(IFNAMES_INSTALL) $(INITSCRIPT_INSTALL) 

TOOLS := ../tools/
VDEVD := ../vdevd/

GEN_IFNAMES := $(BUILD_VDEVD_HELPERS)/gen-ifnames.sh
MKINITRAMFS := make-initramfs.sh
VDEV_INSTALL_SCRIPT := vdev-install.sh

# debugging
print-%: ; @echo $*=$($*)

all: $(VDEV_CONFIG_BUILD)
$(CONF_BUILD): vdevd.conf.in
	@mkdir -p $(shell dirname "$@")
	@cat "$<" | \
		sed -e 's~@PREFIX@~$(PREFIX)~g;' \
		    -e 's~@CONF_DIR@~$(ETCDIR_VDEV)~g;' \
		    -e 's~@RUNDIR@~$(RUNDIR_VDEV)~g;' \
		    -e 's~@LOGDIR@~$(LOGDIR_VDEV)~g;' > "$@"

$(IFNAMES_BUILD): ifnames.conf.in 
	@mkdir -p $(shell dirname "$@")
	@cat "$<" > "$@"
	$(GEN_IFNAMES) >> "$@"

$(INITSCRIPT_BUILD): sysv-initscript.sh 
	@mkdir -p $(shell dirname "$@")
	@cp -a "$<" "$@"

$(BUILD_VDEV_CONFIG)/actions/%: actions/%
	@mkdir -p $(shell dirname "$@")
	@cat "$<" > "$@"

$(BUILD_VDEV_INITRAMFS)/%: initramfs/%
	@mkdir -p $(shell dirname "$@")
	@cp -a "$<" "$@"

$(CONF_INSTALL): $(CONF_BUILD)
	@mkdir -p $(shell dirname "$@")
	@cat "$<" > "$@"

$(IFNAMES_INSTALL): $(IFNAMES_BUILD)
	@mkdir -p $(shell dirname "$@")
	@cat "$<" > "$@"

$(INITSCRIPT_INSTALL): $(INITSCRIPT_BUILD)
	@mkdir -p $(shell dirname "$@")
	@cp -a "$<" "$@"

$(INSTALL_VDEV_CONFIG)/actions/%: $(BUILD_VDEV_CONFIG)/actions/%
	@mkdir -p $(shell dirname "$@")
	@cat "$<" > "$@"

$(INSTALL_VDEV_INITRAMFS)/%: $(BUILD_VDEV_INITRAMFS)/%
	@mkdir -p $(shell dirname "$@")
	@cp -a "$<" "$@"

.PHONY: initramfs
initramfs: $(VDEV_CONFIG_INSTALL) $(INITSCRIPT_INSTALL)
	$(SHELL) ./$(MKINITRAMFS)

.PHONY: install
install: $(VDEV_CONFIG_INSTALL)

.PHONY: install-initscript
install-initscript: $(VDEV_CONFIG_INSTALL)
	$(SHELL) ./$(VDEV_INSTALL_SCRIPT) $(DESTDIR)$(PREFIX)

.PHONY: clean
clean:
	rm -f $(VDEV_CONFIG_BUILD)

.PHONY: uninstall
uninstall:
	rm -f $(VDEV_CONFIG_INSTALL)
	update-rc.d -f vdev remove || true
	@echo ""
	@echo "vdev configuration and init scripts have been removed."
	@echo "You will need to re-enable your previous device manager."
	@echo ""
	@echo "Example (on Debian systems):"
	@echo "   $$ sudo update-rc.d udev defaults"
	@echo "   $$ sudo update-rc.d udev-finish defaults"
	@echo "   $$ sudo insserv"
	@echo ""
