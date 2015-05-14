include ../buildconf.mk

C_SRCS:= $(wildcard *.c) $(wildcard os/*.c)

LIB   := -lvdev
OBJ   := $(patsubst %.c,$(BUILD_VDEVD)/%.o,$(C_SRCS)) 
VDEVD := $(BUILD_VDEVD)/vdevd

VDEVD_INSTALL := $(INSTALL_VDEVD)/vdevd

HELPERS_DIR := helpers/$(OS)

all: $(VDEVD) helpers

$(VDEVD): $(OBJ) helpers
	@mkdir -p $(shell dirname "$@")
	$(CC) -o $@ $(OBJ) $(LIBINC) $(LIB)

$(BUILD_VDEVD)/%.o : %.c
	@mkdir -p $(shell dirname "$@")
	$(CC) -o $@ -c $< 

.PHONY: helpers
helpers:
	$(MAKE) -C $(HELPERS_DIR)

.PHONY: helpers-install
helpers-install:
	$(MAKE) -C $(HELPERS_DIR) install

install: $(VDEVD_INSTALL) helpers-install
$(VDEVD_INSTALL): $(VDEVD)
	@mkdir -p $(INSTALL_VDEVD)
	cp -a $(VDEVD) $(INSTALL_VDEVD)/

.PHONY: clean
clean:
	rm -f $(OBJ) $(VDEVD)
	$(MAKE) -C $(HELPERS_DIR) clean

.PHONY: uninstall
uninstall:
	rm -f $(VDEVD)
	$(MAKE) -C $(HELPERS_DIR) uninstall
