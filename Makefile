all:
	$(MAKE) -C libvdev
	$(MAKE) -C vdevd
	$(MAKE) -C fs

install:
	$(MAKE) -C libvdev install
	$(MAKE) -C vdevd install
	$(MAKE) -C fs install

clean:
	$(MAKE) -C libvdev clean
	$(MAKE) -C vdevd clean
	$(MAKE) -C fs clean
