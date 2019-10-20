CC=clang-8 -mavx2 -march=native -g -O2
sd : sd.o
	$(CC) -o sd sd.o

sd.o : sd.c sd.h dict.h
	 $(CC) -c sd.c

clean:
	rm -f sd.o sd

all: sd
