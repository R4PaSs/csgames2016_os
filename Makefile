all: coder decoder
	mkdir -p bin
	gcc -g src/filecoder.o src/utils.o -o bin/filecoder
	gcc -g src/filedecoder.o src/utils.o -o bin/filedecoder

coder: utils
	gcc -g -c src/filecoder.c -o src/filecoder.o

decoder: utils
	gcc -g -c src/filedecoder.c -o src/filedecoder.o

utils:
	gcc -g -c src/utils.c -o src/utils.o

clean:
	rm bin/*
	rm src/*.o
