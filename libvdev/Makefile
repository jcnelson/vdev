include ../buildconf.mk

C_SRCS:= $(wildcard *.c)

HEADERS := $(wildcard *.h)
OBJ := $(patsubst %.c,$(BUILD_LIBVDEV)/%.o,$(C_SRCS))
LIB :=

VERSION_MAJOR := 1
VERSION_MINOR := 0
VERSION_PATCH := 1

LIBVDEV := $(BUILD_LIBVDEV)/libvdev.so
LIBVDEV_SONAME := libvdev.so.$(VERSION_MAJOR)
LIBVDEV_SO := $(BUILD_LIBVDEV)/libvdev.so.$(VERSION_MAJOR)
LIBVDEV_LIB := $(BUILD_LIBVDEV)/libvdev.so.$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

LIBVDEV_FILES := $(LIBVDEV) $(LIBVDEV_SO) $(LIBVDEV_LIB)

LIBVDEV_HEADERS := $(patsubst %.h,$(BUILD_LIBVDEV_HEADERS)/%.h,$(HEADERS))

LIBVDEV_FILES_INSTALL := $(patsubst $(BUILD_LIBVDEV)/%,$(LIBDIR)/%,$(LIBVDEV_FILES))

LIBVDEV_HEADERS_INSTALL := $(patsubst $(BUILD_LIBVDEV_HEADERS)/%.h,$(INCLUDEDIR)/libvdev/%.h,$(LIBVDEV_HEADERS))

all: $(LIBVDEV_FILES) $(LIBVDEV_HEADERS)

$(LIBVDEV): $(OBJ)
	@mkdir -p $(shell dirname "$@")
	$(CPP) -shared -Wl,-soname,$(LIBVDEV_SONAME) -o $@ $(OBJ) $(LIBINC) $(LIB)

$(LIBVDEV_LIB): $(LIBVDEV)
	@mkdir -p $(shell dirname "$@")
	@ln -s $(shell basename $<) $@

$(LIBVDEV_SO): $(LIBVDEV_LIB)
	@mkdir -p $(shell dirname "$@")
	@ln -s $(shell basename $<) $@

$(BUILD_LIBVDEV_HEADERS)/%.h: %.h
	@mkdir -p $(shell dirname "$@")
	@cp -a $< $@

$(BUILD_LIBVDEV)/%.o: %.c
	@mkdir -p $(shell dirname "$@")
	$(CPP) -o $@ -c $< 

install: $(LIBVDEV_FILES_INSTALL) $(LIBVDEV_HEADERS_INSTALL)

$(LIBDIR)/%: $(BUILD_LIBVDEV)/%
	@mkdir -p $(shell dirname "$@")
	@cp -a $< $@

$(INCLUDEDIR)/libvdev/%.h: $(BUILD_LIBVDEV_HEADERS)/%.h
	@mkdir -p $(shell dirname "$@")
	@cp -a $< $@

.PHONY: uninstall
uninstall:
	rm -f $(LIBVDEV_FILES_INSTALL) $(LIBVDEV_HEADERS_INSTALL)

.PHONY: clean 
clean:
	rm -f $(LIBVDEV_FILES) $(LIBVDEV_HEADERS) $(OBJ)
