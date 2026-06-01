

# top-level makefile

OSGHOME ?= /usr/local

all:
	cd swwreader; make OSGHOME=$(OSGHOME)
	cd viewer; make OSGHOME=$(OSGHOME)

clean:
	cd swwreader; make clean
	cd viewer; make clean
	cd tests; make clean

test:
	cd tests; make test OSGHOME=$(OSGHOME)

install:
	cd swwreader; make install OSGHOME=$(OSGHOME)

# AppImage build — requires linuxdeploy-x86_64.AppImage in the project root.
# env -i strips any active conda environment to prevent ABI mismatches that
# cause the resulting AppImage to segfault on launch.
LINUXDEPLOY ?= ./linuxdeploy-x86_64.AppImage

appimage:
	env -i HOME=$(HOME) PATH=/usr/bin:/bin:/usr/local/bin $(MAKE) OSGHOME=/usr
	cp bin/anuga_viewer AppDir/usr/bin/anuga_viewer
	# Clear bundled libs so stale deps from previous builds are not carried forward.
	rm -rf AppDir/usr/lib && mkdir -p AppDir/usr/lib
	cp bin/libswwreader.so AppDir/usr/lib/libswwreader.so
	env -i HOME=$(HOME) PATH=/usr/bin:/bin:/usr/local/bin DISPLAY=$(DISPLAY) \
	    LD_LIBRARY_PATH=$(CURDIR)/bin \
	    $(LINUXDEPLOY) --appdir AppDir --executable bin/anuga_viewer --output appimage

