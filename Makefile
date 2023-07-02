TARGET ?= snander
CFLAGS := -Wall -s
FILES := main.o flashcmd_api.o mstar_spi.o spi_controller.o spi_nand_flash.o spi_nor_flash.o timer.o

all: clean $(TARGET)

$(TARGET): $(FILES)
	$(CC) $(CFLAGS) $(FILES) -o $(TARGET)

clean:
	rm -f *.o $(TARGET)