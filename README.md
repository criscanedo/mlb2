Minimal Linux Bootloader 2
========================

Minimal Linux Bootloader, adapted/adopted from Sebastian Plotz
by Wiktor Kerr, and then later adapted for use in virtualized
booting environments by Phil Hofer for use in the
[distill project](https://git.sr.ht/~pmh/distill).

This is a single stage x86 bootloader that can boot a single Linux kernel.
There is no support for initrd and only one kernel can be configured to boot at
any time.

MLB was originally written by Sebastian Plotz. Wiktor Kerr changed the code very
slightly and wrote the original Makefile and `mlbinstall`. Phil Hofer optimized
the assembly a bit more and wrote a new Makefile and `mlb2install`.

http://sebastian-plotz.blogspot.de/

Dependencies
------------

 - a C compiler
 - ld(1)
 - make(1)
 - nasm(1)

Building
--------

Run `make`, optionally with CC, LD, CFLAGS, LDFLAGS, etc. set via the
command line, i.e. `make CC=gcc 'CFLAGS=-02'`

The `mlb2install` binary has the MBR boot code embedded in it, so
it can simply be copied to its final destination, or you can
run `make install`. (The Makefile will respect the conventional
`PREFIX` and `DESTDIR` arguments for the install prefix and staging
prefix, respectively.)

Installing
----------

To install MLB, simply invoke

    mlb2install <target> <kernel-lba> <command line>

where:

 - `<target>` is where you want your bootloader installed (e.g. a file, a block
   device, whatever)
 - `<kernel-lba>` is the LBA (in 512-byte sectors) of the kernel within the
   target device
 - `<command line>` is the command line to pass to the kernel - don't forget to
   pass `root=` as `mlbinstall` will not calculate the root device. Currently
   the command line cannot be longer than 99 bytes (more just won't fit in the
   MBR).

Typically, the kernel will live in reserved (unformatted) space at the
beginning of the drive before the first real MBR partition, or it will
begin at the first partition. (You can simply `dd` your kernel to the
right place once you've arranged the partitions so that it won't be clobbered
by other disk manipulation.)

`mlb2install` is quite noisy if it detects any problems. Restore your MBR from
backup, fix the problems, and try again. If it succeeds, it won't say anything.

Error codes
-----------

If MLB fails to boot your kernel, it will print a one-letter error code.
Currently, there are two error codes:

    Error What it means
    ----- ----------------------------------------
      R   Failed reading data from disk
      M   Failed moving data to its final location
