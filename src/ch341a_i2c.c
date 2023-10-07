#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libusb.h>
#include "spi_controller.h"

#define USB_EP_WRITE 0x02
#define USB_EP_READ 0x82

#define CH341A_I2C_20KHZ 0
#define CH341A_I2C_100KHZ 1
#define CH341A_I2C_400KHZ 2
#define CH341A_I2C_750KHZ 3

#define CH341A_CMD_I2C_STREAM 0xAA
#define CH341A_CMD_I2C_STM_END 0x00
#define CH341A_CMD_I2C_STM_SET 0x60
#define CH341A_CMD_I2C_STM_STA 0x74
#define CH341A_CMD_I2C_STM_STO 0x75
#define CH341A_CMD_I2C_STM_OUT 0x80
#define CH341A_CMD_I2C_STM_IN 0xC0

#define CH341A_USB_VENDOR 0x1A86
#define CH341A_USB_PRODUCT 0x5512

#define MSTAR_DELAY 10
#define MSTAR_PORT 0x49
#define MSTAR_WRITE 0x10
#define MSTAR_READ 0x11
#define MSTAR_END 0x12

static struct libusb_device_handle *handle = NULL;
static int mstar_err = 0;

static int ch341a_i2c_transfer(uint8_t type, uint8_t *buf, int len) {
	int size = 0;
	int ret = 0;

	ret = libusb_bulk_transfer(handle, type, buf, len, &size, 0);
	if (ret < 0) {
		printf("Failed to transfer %d bytes: %s\n", len, strerror(ret));
		return -1;
	}

	return size;
}

static int ch341a_i2c_read(uint8_t *data, uint32_t len) {
	uint8_t buf[32];
	uint8_t sz = 0;
	int ret = 0;

	buf[sz++] = CH341A_CMD_I2C_STREAM;
	buf[sz++] = CH341A_CMD_I2C_STM_STA;
	buf[sz++] = CH341A_CMD_I2C_STM_OUT | 1;
	buf[sz++] = (MSTAR_PORT << 1) | 1;

	if (len > 1) {
		buf[sz++] = CH341A_CMD_I2C_STM_IN | (len - 1);
	}

	buf[sz++] = CH341A_CMD_I2C_STM_IN;
	buf[sz++] = CH341A_CMD_I2C_STM_STO;
	buf[sz++] = CH341A_CMD_I2C_STM_END;

	ret = ch341a_i2c_transfer(USB_EP_WRITE, buf, sz);
	if (ret < 0) {
		return -1;
	}

	ret = ch341a_i2c_transfer(USB_EP_READ, buf, len);
	if (ret < 0) {
		return -1;
	}

	memcpy(data, buf, len);

	return 0;
}

static int ch341a_i2c_write(uint8_t *data, uint32_t len) {
	uint8_t buf[32];
	uint8_t sz = 0;
	int ret = 0;

	buf[sz++] = CH341A_CMD_I2C_STREAM;
	buf[sz++] = CH341A_CMD_I2C_STM_STA;
	buf[sz++] = CH341A_CMD_I2C_STM_OUT | (len + 1);
	buf[sz++] = MSTAR_PORT << 1;

	memcpy(&buf[sz], data, len);
	sz += len;
	buf[sz++] = CH341A_CMD_I2C_STM_STO;
	buf[sz++] = CH341A_CMD_I2C_STM_END;

	ret = ch341a_i2c_transfer(USB_EP_WRITE, buf, sz);
	if (ret < 0) {
		return -1;
	}

	return 0;
}

static int ch341a_i2c_send_command(uint32_t writecnt, uint32_t readcnt,
		const uint8_t *writearr, uint8_t *readarr) {
	if (readcnt) {
		usleep(MSTAR_DELAY);
		uint8_t cmd = MSTAR_READ;
		if (ch341a_i2c_write(&cmd, 1) < 0) {
			mstar_err++;
			return -1;
		}

		usleep(MSTAR_DELAY);
		if (ch341a_i2c_read(readarr, readcnt) < 0) {
			mstar_err++;
			return -1;
		}
	}

	if (writecnt) {
		uint8_t buffer[32];
		usleep(MSTAR_DELAY);
		buffer[0] = MSTAR_WRITE;
		memcpy(buffer + 1, writearr, writecnt);
		if (ch341a_i2c_write(buffer, writecnt + 1) < 0) {
			mstar_err++;
			return -1;
		}
	}

	return 0;
}

static int ch341a_i2c_release(bool enable) {
	usleep(MSTAR_DELAY);
	uint8_t cmd = MSTAR_END;
	if (ch341a_i2c_write(&cmd, 1) < 0) {
		mstar_err++;
		return -1;
	}

	return 0;
}

static int ch341a_i2c_init(const char *options) {
	int ret = libusb_init(NULL);
	if (ret < 0) {
		printf("Could not initialize libusb\n");
		return -1;
	}

#if LIBUSB_API_VERSION >= 0x01000106
	libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, 3);
#else
	libusb_set_debug(NULL, 3);
#endif

	handle = libusb_open_device_with_vid_pid(NULL, CH341A_USB_VENDOR, CH341A_USB_PRODUCT);
	if (handle == NULL) {
		printf("Error opening usb device [%X:%X]\n", CH341A_USB_VENDOR, CH341A_USB_PRODUCT);
		return -1;
	}

	ret = libusb_claim_interface(handle, 0);
	if (ret) {
		printf("Failed to claim interface: %s\n", libusb_error_name(ret));
		goto close_handle;
	}

	uint8_t buf[] = {
		CH341A_CMD_I2C_STREAM,
		CH341A_CMD_I2C_STM_SET | CH341A_I2C_100KHZ,
		CH341A_CMD_I2C_STM_END
	};

	ret = ch341a_i2c_transfer(USB_EP_WRITE, buf, sizeof(buf));
	if (ret < 0) {
		printf("Failed to configure stream interface\n");
		goto close_handle;
	}

	uint8_t cmd[] = {'M', 'S', 'T', 'A', 'R'};
	if (ch341a_i2c_write(cmd, 5) < 0) {
		if (ch341a_i2c_release(false) < 0) {
			goto close_handle;
		}
	}

	return 0;

close_handle:
	libusb_close(handle);
	libusb_exit(NULL);
	handle = NULL;

	return -1;
}

static int ch341a_i2c_shutdown(void) {
	if (mstar_err) {
		printf("Total read/write errors: %d\n", mstar_err);
	}

	libusb_release_interface(handle, 0);
	libusb_close(handle);
	libusb_exit(NULL);
	handle = NULL;

	return 0;
}

const struct spi_controller ch341a_i2c_ctrl = {
	.name = CH341A_I2C_DEVICE,
	.init = ch341a_i2c_init,
	.shutdown = ch341a_i2c_shutdown,
	.send_command = ch341a_i2c_send_command,
	.cs_release = ch341a_i2c_release,
};
