CC = gcc
DLIBS = -lwiringPi
LIB_DIR = lib/
INC_DIR = src/
SRCS = $(wildcard *.c)
all: reader
.PHONY: clean
reader: main.o pn532.o pn532_rpi.o
	$(CC) -Wall -o $@ $^ $(DLIBS)
main.o: $(INC_DIR)main.c config.h
	$(CC) -Wall -c $^ $(DLIBS) -I./ -I$(INC_DIR) -I$(LIB_DIR) -w
pn532.o pn532_rpi.o: $(LIB_DIR)pn532.c $(LIB_DIR)pn532_rpi.c
	$(CC) -Wall -c $(LIB_DIR)pn532.c
	$(CC) -Wall -c $(LIB_DIR)pn532_rpi.c -I$(INC_DIR) -I./
config.h: config.hh
	sed -e 's/@VERSION@/0.1.0/g' -e 's/@PROJECT@/reader/g' config.hh > config.h
clean:
	rm *.o reader config.h config.h.gch
