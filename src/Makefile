CC=clang
#CC=clang-8
#CC=gcc-7
#CFLAGS=-march=native -Wall -g -O2 -fstrict-aliasing -Wcast-qual -fno-exceptions -std=c99 -pedantic -Wextra -Wshadow
CFLAGS=-march=native -Wall -ggdb -O2 -g -Wcast-qual -std=c11 -pedantic -Wextra -Wshadow
#CFLAGS=-Wall -ggdb -g -O0 -std=c99
sd : sd.o
	$(CC) $(CFLAGS) -o sd sd.o

sd.o : sd.c sd.h dict.h
	 $(CC) $(CFLAGS) -c sd.c

clean:
	rm -f sd.o sd

all: sd
