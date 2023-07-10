/*
 * This file was part of the flashrom project.
 *
 * Copyright (C) 2014 Alexandre Boeglin <alex@boeglin.org>
 * Copyright (C) 2021 Daniel Palmer <daniel@thingy.jp>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "spi_controller.h"

#define MSTAR_PORT 0x49
#define MSTAR_WRITE 0x10
#define MSTAR_READ 0x11
#define MSTAR_END 0x12

#define MSTAR_DELAY 10
static u8 buffer[4096];
static int mstar_fd = 0;
static int mstar_err = 0;

static int mstar_spi_end_command(bool enable) {
	usleep(MSTAR_DELAY);
	u8 cmd = MSTAR_END;
	if (write(mstar_fd, &cmd, 1) < 0) {
		mstar_err++;
		return -1;
	}

	return 0;
}

static int mstar_spi_init(const char *i2c_device) {
	mstar_fd = open(i2c_device, O_RDWR);
	if (mstar_fd < 0) {
		printf("Error opening %s: %s\n", i2c_device, strerror(errno));
		return -1;
	}

	printf("Connection: %s, transfer size: %dB\n", i2c_device, max_transfer);

	if (ioctl(mstar_fd, I2C_SLAVE, MSTAR_PORT) < 0) {
		printf("Error setting address 0x%X: %s\n", MSTAR_PORT, strerror(errno));
		close(mstar_fd);
		return -1;
	}

	u8 cmd[] = {'M', 'S', 'T', 'A', 'R'};
	if (write(mstar_fd, cmd, 5) < 0) {
		if (mstar_spi_end_command(false) < 0) {
			close(mstar_fd);
			return -1;
		}
	}

	return 0;
}

static int mstar_spi_shutdown(void) {
	if (mstar_err) {
		printf("Total read/write errors: %d\n", mstar_err);
	}
	close(mstar_fd);

	return 0;
}

static int mstar_spi_send_command(unsigned int writecnt, unsigned int readcnt,
				const unsigned char *writearr, unsigned char *readarr) {
	if (readcnt) {
		usleep(MSTAR_DELAY);
		u8 cmd = MSTAR_READ;
		if (write(mstar_fd, &cmd, 1) < 0) {
			mstar_err++;
			return -1;
		}

		usleep(MSTAR_DELAY);
		if (read(mstar_fd, readarr, readcnt) < 0) {
			mstar_err++;
			return -1;
		}
	} else {
		usleep(MSTAR_DELAY);
		buffer[0] = MSTAR_WRITE;
		memcpy(buffer + 1, writearr, writecnt);
		if (write(mstar_fd, buffer, writecnt + 1) < 0) {
			mstar_err++;
			return -1;
		}
	}

	return 0;
}

const struct spi_controller mstar_spi_ctrl = {
	.name = MSTAR_DEVICE,
	.init = mstar_spi_init,
	.shutdown = mstar_spi_shutdown,
	.send_command = mstar_spi_send_command,
	.cs_release = mstar_spi_end_command,
};
