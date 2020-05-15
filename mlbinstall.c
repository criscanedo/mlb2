/*
 * mlbinstall
 * Configures MLB to boot a kernel with a command line and installs it on
 * a target (e.g. a file, a block device, ...).
 *
 * Copyright (C) 2014 Wiktor W Brodlo
 *
 * mlbinstall is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mlbinstall is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mlbinstall. If not, see <http://www.gnu.org/licenses/>.
 */

#define _BSD_SOURCE

#include <assert.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <err.h>
#include <endian.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

extern char _binary_mlb_bin_start;
extern char _binary_mlb_bin_end;

static const char *
mlb_bin(void)
{
	return (const char *)&_binary_mlb_bin_start;
}

static size_t
mlb_bin_len(void)
{
	size_t diff = (size_t)(&_binary_mlb_bin_end - &_binary_mlb_bin_start);
	if (diff > 510)
		errx(1, "mlb binary section too large: %llu", (unsigned long long)diff);
	if (!diff)
		errx(1, "mlb binary section empty?");
	return diff;
}

/* Checks if the kernel boot protocol is supported */
static void check_version(const char *target, uint32_t kernlba)
{
	int fd = open(target, O_RDONLY);
	if (fd == -1)
		err(1, "Failed opening %s", target);

	long ps = sysconf(_SC_PAGESIZE);
	size_t mlength = (512 / ps + 1) * ps;
	uint8_t *m = mmap(NULL, mlength, PROT_READ, MAP_PRIVATE, fd, (off_t)kernlba << 9);
	if (m == MAP_FAILED)
		err(1, "Failed mapping %s", target);

	/* TODO - portability */
	uint32_t header = *(uint32_t *)(m + 0x202);
	uint16_t version = be16toh(*(uint16_t *)(m + 0x206));
	uint8_t loadflags = m[0x211];

	if (header != 0x53726448)
		errx(1, "%s is missing a Linux kernel header", target);
	if (version < 0x204)
		errx(1, "Kernel too old, boot protocol version >= 0x204/\
kernel version >= 2.6.14 required, but %s is 0x%x", target, version);
	if (!(loadflags & 0x01))
		errx(1, "Kernel needs to be loaded high");

	munmap(m, mlength);
	close(fd);
}

/* Returns the length of cmdline, including the terminating null. */
static uint16_t cmdlen(const char *cmdline, size_t mlblen, size_t mbrlen)
{
	/* Note the last byte of mlb.bin is 0, reserved for cmdline. */
	assert(mbrlen > mlblen);
	size_t maxlen = mbrlen - mlblen + 1;
	size_t len = strnlen(cmdline, maxlen);
	if (len >= maxlen)
		errx(1, "Command line too long, max length: %lu", maxlen);
	return (uint16_t)(len + 1);
}

/* Copies the kernel command line to the MBR buffer. */
static void cmdcopy(uint8_t *mbr, size_t mlblen, const char *cmd, uint16_t clen)
{
	assert(clen < (510 - mlblen));
	memmove(mbr + mlblen - 1, cmd, clen);
	bool sawit = false;
	for (size_t i = 0; i < mlblen - 1; ++i) {
		if (mbr[i] == 0xca && mbr[i + 1] == 0xfe) {
			mbr[i+0] = (uint8_t)(clen & 0xff);
			mbr[i+1] = (uint8_t)((clen >> 8) & 0xff);
			sawit = true;
		}
	}
	if (!sawit)
		errx(1, "didn't see 0xcafe cmdline length sigil");
}

/* Writes the MBR buffer to the target MBR. */
static void mbrwrite(const char *target, uint8_t *mbr)
{
	unsigned char buf[2];
	int fd = open(target, O_WRONLY);
	if (fd == -1)
		err(1, "Failed opening %s", target);
	if (pwrite(fd, mbr, 446, 0) != 446)
		err(1, "%s: failed writing MBR", target);
	buf[0] = 0x55;
	buf[1] = 0xaa;
	if (pwrite(fd, buf, 2, 510) != 2)
		err(1, "%s: failed writing MBR boot magic", target);
	close(fd);
}

int main(int argc, char **argv)
{
	const char *target, *kern, *cmdline;
	uint8_t mbr[510];

	if (argc != 4)
		errx(1, "Usage: %s <target> <kernel> <command line> \n\
Configures MLB to boot the kernel with the command line and installs it on\n\
target (could be a file, a block device, ...). Specify -vbr as the last\n\
argument to not reserve space for a partition table and gain an extra\n\
64 bytes for the command line.\n", argv[0]);

	target = argv[1];
	kern = argv[2];
	cmdline = argv[3];

	size_t mbr_len = 446;
	size_t mlb_len = mlb_bin_len();
	assert(mlb_len < mbr_len);
	const char *mlb = mlb_bin();
	uint16_t cmdline_len = cmdlen(cmdline, mlb_len, (uint16_t)mbr_len);
	uint32_t lba = (uint32_t)strtoul(kern, NULL, 0);

	check_version(target, lba);

	memset(mbr, 0, sizeof(mbr));
	/* for reasons I cannot discern, gcc 9.3.0
	 * triggers a SIGILL when this is replaced with
	 * a call to memmove() ... */
	for (int i=0; i<mlb_len; i++)
		mbr[i]=mlb[i];
	mbr[mlb_len - 5] = (uint8_t)(lba & 0xff);
	mbr[mlb_len - 4] = (uint8_t)((lba >> 8) & 0xff);
	mbr[mlb_len - 3] = (uint8_t)((lba >> 16) & 0xff);
	mbr[mlb_len - 2] = (uint8_t)((lba >> 24) & 0xff);

	cmdcopy(mbr, mlb_len, cmdline, cmdline_len);

	mbrwrite(target, mbr);
	return 0;
}
