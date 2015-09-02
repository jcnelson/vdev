include buildconf.mk

.PHONY: all
all:
	$(MAKE) -C vdevd
	$(MAKE) -C fs
	$(MAKE) -C example
	$(MAKE) -C hwdb

.PHONY: install
install:
	$(MAKE) -C vdevd install
	$(MAKE) -C fs install
	$(MAKE) -C example install
	$(MAKE) -C hwdb install

.PHONY: clean
clean:
	$(MAKE) -C vdevd clean
	$(MAKE) -C fs clean
	$(MAKE) -C example clean
	$(MAKE) -C hwdb clean

.PHONY: uninstall
uninstall:
	$(MAKE) -C vdevd uninstall
	$(MAKE) -C fs uninstall
	$(MAKE) -C example uninstall
	$(MAKE) -C hwdb uninstall
