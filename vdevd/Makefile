include ../buildconf.mk

C_SRCS := $(wildcard *.c) $(wildcard os/*.c) $(wildcard ../libvdev/*.c)

LIB   := -lpthread -lrt
OBJ   := $(patsubst %.c,$(BUILD_VDEVD)/%.o,$(C_SRCS))
VDEVD := $(BUILD_VDEVD)/vdevd

VDEVD_INSTALL := $(INSTALL_VDEVD)/vdevd

HELPERS_DIR := helpers/$(OS)

all: $(VDEVD) helpers

$(VDEVD): $(OBJ) helpers
	@mkdir -p "$(shell dirname "$@")"
	$(CC) -o $@ $(OBJ) $(LIB) $(LIBINC) $(LDFLAGS)

$(BUILD_VDEVD)/%.o : %.c
	@mkdir -p "$(shell dirname "$@")"
	$(CC) $(CFLAGS) $(DEFS) $(INC) -o "$@" -c "$<"

$(BUILD_LIBVDEV)/%.o: $(LIBVDEV_ROOT)/%.c
	@mkdir -p "$(shell dirname "$@")"
	$(CC) $(CFLAGS) $(DEFS) $(INC) -o "$@" -c "$<"

.PHONY: helpers
helpers:
	$(MAKE) -C $(HELPERS_DIR)

.PHONY: helpers-install
helpers-install:
	$(MAKE) -C $(HELPERS_DIR) install

install: $(VDEVD_INSTALL) helpers-install
$(VDEVD_INSTALL): $(VDEVD)
	@mkdir -p $(INSTALL_VDEVD)
	cp -a "$<" "$@"

.PHONY: clean
clean:
	rm -f $(OBJ) $(VDEVD)
	$(MAKE) -C $(HELPERS_DIR) clean

.PHONY: uninstall
uninstall:
	rm -f $(VDEVD)
	$(MAKE) -C $(HELPERS_DIR) uninstall
