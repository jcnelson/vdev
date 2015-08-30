include ../buildconf.mk

C_SRCS:= $(wildcard *.c) $(wildcard ../libvdev/*.c)
CXX_SRCS := $(wildcard *.cpp)

DEFS  := $(DEFS) -D_FILE_OFFSET_BITS=64
LIB   := -lfuse -lfskit_fuse -lfskit -lpstat
OBJ   := $(patsubst %.c,$(BUILD_VDEVFS)/%.o,$(C_SRCS)) \
	      $(patsubst %.cpp,$(BUILD_VDEVFS)/%.o,$(CXX_SRCS))

VDEVFS := $(BUILD_VDEVFS)/vdevfs

VDEVFS_INSTALL := $(INSTALL_VDEVFS)/vdevfs

all: $(VDEVFS)

$(VDEVFS): $(OBJ)
	@mkdir -p "$(shell dirname "$@")"
	$(CXX) $(LDFLAGS) -o "$@" $(OBJ) $(LIBINC) $(LIB)

$(BUILD_VDEVFS)/%.o : %.c
	@mkdir -p "$(shell dirname "$@")"
	$(CC) $(CFLAGS) $(INC) $(DEFS) -o "$@" -c "$<"

$(BUILD_VDEVFS)/%.o : %.cpp
	@mkdir -p "$(shell dirname "$@")"
	$(CXX) $(CXXFLAGS) $(INC) $(DEFS) -o "$@" -c "$<"

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
