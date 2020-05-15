CC ?= gcc
CFLAGS ?= -O2 -pedantic -Wall -fno-strict-aliasing
LDFLAGS ?=
LD ?= ld
NASM ?= nasm
VERSION := 0.2

.PHONY: all install clean tarball

all: mlb2install

mlb.bin: mlb.asm
	$(NASM) -o $@ $<

mlb.o: mlb.bin
	$(LD) -r -b binary $< -o $@

mlb2install: mlb.o mlbinstall.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

mlb2-$(VERSION).tar.zst:
	git archive --format=tar --prefix=mlb2-$(VERSION)/ HEAD | zstd -c -o $@

install: mlb2install
	install -D -m 755 -t $(DESTDIR)$(PREFIX)/sbin mlb2install

tarball: mlb2-$(VERSION).tar.zst

clean:
	$(RM) mlb2install mlb.bin mlb.o
