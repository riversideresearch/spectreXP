.PHONY: all clean

all:
	make -C libmicroarchi all
	make -C poc all

clean:
	@rm -f *.o victim attack
	make -C libmicroarchi clean
	make -C poc clean
