
LIC=gpl3
#EMAIL=ralph.ronnquist@gmail.com
EMAIL=pete.gossner@gmail.com
VERSION=1.0alpha-petes
ARCH=$(shell uname -r)
HOMEPAGE="github.com/PeteGozz/vdev"

DEBS += vdevd_$(VERSION)_$(ARCH).deb
DEBS += libudev1-compat_$(VERSION)_$(ARCH).deb
DEBS += vdev_$(VERSION)_$(ARCH).deb

all: $(DEBS)

vdevd_$(VERSION)_$(ARCH).deb:
	$(MAKE) -C vdevd -f ../debian.mk deb PACKAGE=vdevd

libudev1-compat_$(VERSION)_$(ARCH).deb: 
	$(MAKE) -C libudev-compat -f ../debian.mk deb PACKAGE=libudev1-compat

vdev_$(VERSION)_$(ARCH).deb:
	$(MAKE) -C example -f ../debian.mk deb PACKAGE=vdev

vdevfs_$(VERSION)_$(ARCH).deb:
	$(MAKE) -C fs -f ../debian.mk deb PACKAGE=vdevfs

.PHONY: deb
deb:
	dh_make -a -s -n -y -e "$(EMAIL)" -p $(PACKAGE)_$(VERSION) -c $(LIC)
	PREFIX= INCLUDE_PREFIX=/usr dpkg-buildpackage -b -uc

