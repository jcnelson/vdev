all:
	$(MAKE) -C libvdev
	$(MAKE) -C vdevd
	$(MAKE) -C fs
	$(MAKE) -C example

install:
	$(MAKE) -C libvdev install
	$(MAKE) -C vdevd install
	$(MAKE) -C fs install
	$(MAKE) -C example install

clean:
	$(MAKE) -C libvdev clean
	$(MAKE) -C vdevd clean
	$(MAKE) -C fs clean
