CC=clang-8 
#CFLAGS=-march=native -Wall -g -O2 -fstrict-aliasing -Wcast-qual -fno-exceptions -std=c99 -pedantic -Wextra -Wshadow
CFLAGS=-march=native -Wall -g -O2 -Wcast-qual -std=c99 -pedantic -Wextra -Wshadow
sd : sd.o
	$(CC) $(CFLAGS) -o sd sd.o

sd.o : sd.c sd.h dict.h
	 $(CC) $(CFLAGS) -c sd.c

clean:
	rm -f sd.o sd

all: sd
