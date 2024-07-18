CFLAGS=-std=c11 -g -fno-common

ycc:main.o
	$(CC) -o ycc main.o $(LDFLAGS)

test:ycc
	./test.sh

clean:
	rm -f ycc *.o *~ tmp*

.PHONY: test clean
