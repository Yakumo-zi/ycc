CFLAGS=-std=c11 -g -fno-common
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

ycc:$(OBJS)
	$(CC) $(CFLAGS)  -o $@ $^ $(LDFLAGS)
$(OBJS):ycc.h

test:ycc
	./test.sh

clean:
	rm -f ycc *.o *~ tmp*

.PHONY: test clean
