include ../buildconf.mk

CFLAGS := -Wall -g -fPIC -fstack-protector -fstack-protector-all -pthread -Wno-unused-variable -Wno-unused-but-set-variable
INC := $(INC) -I$(BUILD_LIBUDEV_COMPAT_HEADERS)
DEFS := $(DEFS) -D_POSIX_C_SOURCE=200809L -D_BSD_SOURCE -D_GNU_SOURCE
CC   := cc $(CFLAGS) $(INC) $(DEFS)

C_SRCS:= $(wildcard *.c)
SYM_SRCS:= $(wildcard *.sym)

HEADERS := $(wildcard *.h)
OBJ := $(patsubst %.c,$(BUILD_LIBUDEV_COMPAT)/%.o,$(C_SRCS))
OBJ_TEST := $(filter-out $(BUILD_LIBUDEV_COMPAT)/libudev-fs.o,$(OBJ))
SYM_BUILD := $(patsubst %.sym,$(BUILD_LIBUDEV_COMPAT)/%.sym,$(SYM_SRCS))
LIB :=

VERSION_MAJOR := 1
VERSION_MINOR := 5
VERSION_PATCH := 2

LIBUDEV_COMPAT := $(BUILD_LIBUDEV_COMPAT)/libudev.so.$(VERSION_MAJOR)
LIBUDEV_COMPAT_SONAME := libudev.so.$(VERSION_MAJOR)
LIBUDEV_COMPAT_LIB := $(BUILD_LIBUDEV_COMPAT)/libudev.so.$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)
 
LIBUDEV_COMPAT_FILES := $(LIBUDEV_COMPAT) $(LIBUDEV_COMPAT_LIB)

LIBUDEV_COMPAT_HEADERS := $(patsubst %.h,$(BUILD_LIBUDEV_COMPAT_HEADERS)/%.h,$(HEADERS))

# NOTE: only install libudev.h
LIBUDEV_COMPAT_INSTALL_HEADERS := $(BUILD_LIBUDEV_COMPAT_HEADERS)/libudev.h

LIBUDEV_COMPAT_FILES_INSTALL := $(patsubst $(BUILD_LIBUDEV_COMPAT)/%,$(INSTALL_LIBUDEV_COMPAT)/%,$(LIBUDEV_COMPAT_FILES))

LIBUDEV_COMPAT_HEADERS_INSTALL := $(patsubst $(BUILD_LIBUDEV_COMPAT_HEADERS)/%.h,$(INSTALL_LIBUDEV_COMPAT_HEADERS)/%.h,$(LIBUDEV_COMPAT_INSTALL_HEADERS))

all: $(LIBUDEV_COMPAT_FILES) $(LIBUDEV_COMPAT_HEADERS)

test: $(BUILD_LIBUDEV_COMPAT)/libudev-fs 

$(LIBUDEV_COMPAT): $(OBJ) $(SYM_BUILD)
	@mkdir -p $(shell dirname "$@")
	$(CC) -shared -Wl,--version-script=libudev.sym,-soname,$(LIBUDEV_COMPAT_SONAME) -o "$@" $(OBJ) $(LIBINC) $(LIB)

$(LIBUDEV_COMPAT_LIB): $(LIBUDEV_COMPAT)
	@mkdir -p "$(shell dirname "$@")"
	@ln -sf "$(shell basename "$<")" "$@"

$(BUILD_LIBUDEV_COMPAT)/libudev-fs: $(BUILD_LIBUDEV_COMPAT)/libudev-fs-test.o $(OBJ_TEST)
	$(CC) -o "$@" "$<" $(OBJ_TEST) $(LIB) $(LIBINC) 

$(BUILD_LIBUDEV_COMPAT)/libudev-fs-test.o: libudev-fs.c $(LIBUDEV_COMPAT_HEADERS)
	$(CC) -o "$@" -c "$<" -DTEST

$(BUILD_LIBUDEV_COMPAT_HEADERS)/%.h: %.h
	@mkdir -p $(shell dirname "$@")
	cp -a "$<" "$@"

$(BUILD_LIBUDEV_COMPAT)/%.o: %.c $(LIBUDEV_COMPAT_HEADERS)
	@mkdir -p $(shell dirname "$@")
	$(CC) -o "$@" -c "$<"

$(BUILD_LIBUDEV_COMPAT)/%.sym: %.sym
	@mkdir -p $(shell dirname "$@")
	cp -a "$<" "$@"

install: $(LIBUDEV_COMPAT_FILES_INSTALL) $(LIBUDEV_COMPAT_HEADERS_INSTALL)

$(INSTALL_LIBUDEV_COMPAT)/%: $(BUILD_LIBUDEV_COMPAT)/%
	@mkdir -p $(shell dirname "$@")
	test -e "$@" && rm -f "$@"
	cp -a "$<" "$@"

$(INSTALL_LIBUDEV_COMPAT_HEADERS)/%.h: $(BUILD_LIBUDEV_COMPAT_HEADERS)/%.h
	@mkdir -p $(shell dirname "$@")
	cp -a "$<" "$@"

.PHONY: uninstall
uninstall:
	rm -f $(LIBUDEV_COMPAT_FILES_INSTALL) $(LIBUDEV_COMPAT_HEADERS_INSTALL)

.PHONY: clean 
clean:
	rm -f $(LIBUDEV_COMPAT_FILES) $(LIBUDEV_COMPAT_HEADERS) $(OBJ) $(SYM_BUILD)
