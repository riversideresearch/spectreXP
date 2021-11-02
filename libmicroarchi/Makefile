.PHONY: all clean test

CFLAGS := -std=gnu11 -Wall -Wextra -O0
LDFLAGS := -L. -lmicroarchi -Wl,-rpath,.

all: libmicroarchi.so


libmicroarchi.so: libmicroarchi.o
	$(CC) -fPIC -shared -o $@ $^

libmicroarchi.o: libmicroarchi.c libmicroarchi.h
	$(CC) -fPIC $(CPPFLAGS) $(CFLAGS) -o $@ -c $<


test: libmicroarchi_test

libmicroarchi_test: libmicroarchi.so libmicroarchi_test.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ -o $@ $(LDFLAGS) 

libmicroarchi_test.o: libmicroarchi_test.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $^


clean:
	@rm -f *.o *.so libmicroarchi_test
