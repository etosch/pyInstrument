FLAGS := -I/usr/include/python2.7 -lpython2.7

.PHONY : all instrument

all: instrument

instrument:
	sudo rm -rf build
	sudo python setup.py build install

c: 
	clang $(FLAGS) instrument.c -o main
