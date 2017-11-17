GCC=gcc
CFLAGS=-Wall -ggdb -fPIC
#CFLAGS=
all: libfunc test
libfunc:func.o func1.o
%.o:%.c
        $(GCC) -c $(CFLAGS) -o $@ $<
clean:
        rm -fr *.o
