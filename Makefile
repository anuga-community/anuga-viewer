

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


