CC=clang-8 -march=native -Wall -g -O2 -fstrict-aliasing -Wcast-qual -fno-exceptions -std=c99 -pedantic -Wextra -Wshadow -flto
sd : sd.o
	$(CC) -o sd sd.o

sd.o : sd.c sd.h dict.h
	 $(CC) -c sd.c

clean:
	rm -f sd.o sd

all: sd
