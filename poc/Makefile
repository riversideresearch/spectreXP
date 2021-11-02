.PHONY: all clean

LIBMICROARCHI_PATH=../libmicroarchi/

CFLAGS := -std=c11 -Wall -Wextra 
CPPFLAGS := -I$(LIBMICROARCHI_PATH) 
LDFLAGS += -L$(LIBMICROARCHI_PATH) -Wl,-rpath,$(LIBMICROARCHI_PATH) -lmicroarchi

all: attack victim 

attack: attack.o
	$(CC) -O0 $(CFLAGS) $^  -o $@ $(LDFLAGS) 

victim: victim.o
	$(CC) -O2 $(CFLAGS) $^  -o $@ $(LDFLAGS) 

%.o: %.c 
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $^

clean:
	@rm -f attack victim *.o *.so
