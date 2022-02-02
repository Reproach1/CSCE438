server: crsd.c
	gcc -lpthread -o crsd.o crsd.c
	
client: crc.c
	gcc -lpthread -o crc.o crc.c

all: server client

clean:
	rm *.o