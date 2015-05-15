include buildconf.mk

.PHONY: all
all:
	$(MAKE) -C libvdev
	$(MAKE) -C vdevd
	$(MAKE) -C fs
	$(MAKE) -C example
	@echo "PREFIX ?= $(PREFIX)" > "$(INSTALL_MK)"
	@echo "BINDIR ?= $(BINDIR)" >> "$(INSTALL_MK)"
	@echo "SBINDIR ?= $(SBINDIR)" >> "$(INSTALL_MK)"
	@echo "LIBDIR ?= $(LIBDIR)" >> "$(INSTALL_MK)"
	@echo "INCLUDEDIR ?= $(INCLUDEDIR)" >> "$(INSTALL_MK)"
	@echo "USRBINDIR ?= $(USRBINDIR)" >> "$(INSTALL_MK)"
	@echo "USRSBINDIR ?= $(USRSBINDIR)" >> "$(INSTALL_MK)"
	@echo "USRSHAREDIR ?= $(USRSHAREDIR)" >> "$(INSTALL_MK)"
	@echo "PKGCONFIGDIR ?= $(PKGCONFIGDIR)" >> "$(INSTALL_MK)"
	@echo "ETCDIR ?= $(ETCDIR)" >> "$(INSTALL_MK)"

.PHONY: install
install:
	$(MAKE) -C libvdev install
	$(MAKE) -C vdevd install
	$(MAKE) -C fs install
	$(MAKE) -C example install

.PHONY: clean
clean:
	$(MAKE) -C libvdev clean
	$(MAKE) -C vdevd clean
	$(MAKE) -C fs clean
	$(MAKE) -C example clean
	@rm -f $(INSTALL_MK)

.PHONY: uninstall
uninstall:
	$(MAKE) -C libvdev uninstall
	$(MAKE) -C vdevd uninstall
	$(MAKE) -C fs uninstall
	$(MAKE) -C example uninstall
