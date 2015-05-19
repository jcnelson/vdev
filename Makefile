include buildconf.mk

.PHONY: all
all:
	$(MAKE) -C libvdev
	$(MAKE) -C vdevd
	$(MAKE) -C fs
	$(MAKE) -C example

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

.PHONY: uninstall
uninstall:
	$(MAKE) -C libvdev uninstall
	$(MAKE) -C vdevd uninstall
	$(MAKE) -C fs uninstall
	$(MAKE) -C example uninstall
