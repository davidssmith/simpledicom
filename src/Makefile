sd : sd.o
	clang-4.0 -g -O2 -o sd sd.o

sd.o : sd.c sd.h
	clang-4.0 -g -c -O2 sd.c

clean:
	rm -f sd.o sd

all: sd
