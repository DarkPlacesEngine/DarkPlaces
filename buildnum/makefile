all: buildnum

.c.o:
	gcc -Wall -c $*.c

buildnum: buildnum.o
	gcc -o $@ $^

clean:
	-rm -f buildnum.o buildnum *.d

.PHONY: clean

-include *.d
