include ../buildconf.mk

C_SRCS:= $(wildcard *.c) $(wildcard ../libvdev/*.c)
CPP_SRCS := $(wildcard *.cpp)

DEFS  := $(DEFS) -D_FILE_OFFSET_BITS=64
LIB   := -lfuse -lfskit_fuse -lfskit -lpstat
OBJ   := $(patsubst %.c,$(BUILD_VDEVFS)/%.o,$(C_SRCS)) \
	      $(patsubst %.cpp,$(BUILD_VDEVFS)/%.o,$(CPP_SRCS))

VDEVFS := $(BUILD_VDEVFS)/vdevfs

VDEVFS_INSTALL := $(INSTALL_VDEVFS)/vdevfs

all: $(VDEVFS)

$(VDEVFS): $(OBJ)
	@mkdir -p "$(shell dirname "$@")"
	$(CPP) $(LDFLAGS)  -o "$@" $(OBJ) $(LIBINC) $(LIB)

$(BUILD_VDEVFS)/%.o : %.c
	@mkdir -p "$(shell dirname "$@")"
	$(CPP) $(CPPFLAGS) $(INC) $(DEFS) -o "$@" -c "$<"

$(BUILD_VDEVFS)/%.o : %.cpp
	@mkdir -p "$(shell dirname "$@")"
	$(CPP) $(CPPFLAGS) $(INC) $(DEFS) -o "$@" -c "$<"

install: $(VDEVFS_INSTALL)
$(VDEVFS_INSTALL): $(VDEVFS)
	@mkdir -p $(INSTALL_VDEVFS)
	cp -a $(VDEVFS) $(INSTALL_VDEVFS)/

.PHONY: clean
clean:
	rm -f $(OBJ) $(VDEVFS)

.PHONY: uninstall
uninstall:
	rm -f $(VDEVFS_INSTALL)
