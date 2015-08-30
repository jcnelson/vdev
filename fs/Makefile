include ../buildconf.mk

C_SRCS:= $(wildcard *.c) $(wildcard ../libvdev/*.c)

DEFS  := $(DEFS) -D_FILE_OFFSET_BITS=64
LIB   := -lfuse -lfskit_fuse -lfskit -lpstat
OBJ   := $(patsubst %.c,$(BUILD_VDEVFS)/%.o,$(C_SRCS)) \

VDEVFS := $(BUILD_VDEVFS)/vdevfs

VDEVFS_INSTALL := $(INSTALL_VDEVFS)/vdevfs

all: $(VDEVFS)

$(VDEVFS): $(OBJ)
	@mkdir -p "$(shell dirname "$@")"
	$(CC) $(LDFLAGS) -o "$@" $(OBJ) $(LIBINC) $(LIB)

$(BUILD_VDEVFS)/%.o : %.c
	@mkdir -p "$(shell dirname "$@")"
	$(CC) $(CFLAGS) $(INC) $(DEFS) -o "$@" -c "$<"

install: $(VDEVFS_INSTALL)
$(VDEVFS_INSTALL): $(VDEVFS)
	@mkdir -p "$(shell dirname "$@")"
	cp -a "$<" "$@"

.PHONY: clean
clean:
	rm -f $(OBJ) $(VDEVFS)

.PHONY: uninstall
uninstall:
	rm -f $(VDEVFS_INSTALL)
