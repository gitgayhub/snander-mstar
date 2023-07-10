TARGET ?= snander
CFLAGS += -Wall -s
FILES += src/main.o src/ch341a_spi.c src/flashcmd_api.o src/timer.o \
	src/spi_controller.o src/spi_nand_flash.o src/spi_nor_flash.o

ifeq ($(findstring mingw,$(CC)),)
FILES += src/mstar_spi.c
LDFLAGS += $(shell pkg-config libusb-1.0 --libs --cflags)
else
LDFLAGS += -I libusb/libusb -L $(PWD) -lusb-1.0
endif

all: clean $(FILES)
	$(CC) $(CFLAGS) $(FILES) $(LDFLAGS) -o $(TARGET)

clean:
	rm -f src/*.o
