CC_C= arm-linux-gnueabihf-gcc
CCFLAGS= -Wall -g -std=c99 -D _POSIX_C_SOURCE=200809L -Werror -pthread

all: sorter noworky
	cp sorter $(HOME)/cmpt433/public/myApps/
	cp noworky $(HOME)/cmpt433/public/myApps/

%.o : %.c
	$(CC_C) -c $(CCFLAGS) $<

sorter: main.o sorter.o zenController.o udpListener.o
	$(CC_C) $(CCFLAGS) -o sorter main.o sorter.o zenController.o udpListener.o
	
noworky: noworky.o
	$(CC_C) $(CCFLAGS) -o noworky noworky.o

clean:
	rm sorter
	rm noworky
	rm *.o