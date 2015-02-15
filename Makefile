FLAGS := -I/usr/include/python2.7 -lpython2.7 -std=c11

.PHONY : all instrument

all: instrument

instrument:
	rm -rf build
	python setup.py build install --user

c: 
	clang $(FLAGS) instrument.c -o main
