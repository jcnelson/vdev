include buildconf.mk

PREFIX ?= /
INCLUDE_PREFIX ?= /usr

.PHONY: all
all:
	$(MAKE) -C vdevd PREFIX=$(PREFIX)
	$(MAKE) -C fs PREFIX=$(PREFIX)
	$(MAKE) -C hwdb PREFIX=$(PREFIX)
	$(MAKE) -C libudev-compat PREFIX=$(PREFIX) INCLUDE_PREFIX=$(INCLUDE_PREFIX)
	$(MAKE) -C example PREFIX=$(PREFIX)

.PHONY: install
install:
	$(MAKE) -C vdevd install PREFIX=$(PREFIX)
	$(MAKE) -C fs install PREFIX=$(PREFIX)
	$(MAKE) -C hwdb install PREFIX=$(PREFIX)
	$(MAKE) -C libudev-compat install PREFIX=$(PREFIX) INCLUDE_PREFIX=$(INCLUDE_PREFIX)
	$(MAKE) -C example install PREFIX=$(PREFIX)

.PHONY: clean
clean:
	$(MAKE) -C vdevd clean PREFIX=$(PREFIX)
	$(MAKE) -C fs clean PREFIX=$(PREFIX)
	$(MAKE) -C hwdb clean PREFIX=$(PREFIX)
	$(MAKE) -C libudev-compat clean PREFIX=$(PREFIX) INCLUDE_PREFIX=$(INCLUDE_PREFIX)
	$(MAKE) -C example clean PREFIX=$(PREFIX)

.PHONY: uninstall
uninstall:
	$(MAKE) -C vdevd uninstall PREFIX=$(PREFIX)
	$(MAKE) -C fs uninstall PREFIX=$(PREFIX)
	$(MAKE) -C hwdb uninstall PREFIX=$(PREFIX)
	$(MAKE) -C libudev-compat uninstall PREFIX=$(PREFIX) INCLUDE_PREFIX=$(INCLUDE_PREFIX)
	$(MAKE) -C example uninstall PREFIX=$(PREFIX)

