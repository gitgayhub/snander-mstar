TARGET ?= snander
CFLAGS += -Wall -s
FILES += src/main.o src/flashcmd_api.o src/timer.o \
	src/spi_controller.o src/spi_nand_flash.o src/spi_nor_flash.o

ifeq ($(CC),cc)
FILES += src/mstar_spi.c
else
LDFLAGS += -I libusb/libusb -L $(PWD) -lusb-1.0
FILES += src/ch341a_spi.c
endif

all: clean $(FILES)
	$(CC) $(CFLAGS) $(FILES) $(LDFLAGS) -o $(TARGET)

clean:
	rm -f src/*.o
