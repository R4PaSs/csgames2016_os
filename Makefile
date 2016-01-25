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

tests: coder decoder
	./bin/filecoder tests/Test1 -o test1.coal
	./bin/filecoder tests/Test1 -o test1_rol.coal -c rol 3
	./bin/filecoder tests/Test1 -o test1_xor.coal -c xor alsdfuigawef2430-tr78
	./bin/filecoder tests/Compilation_CSG16 -o compil.coal
	./bin/filecoder tests/Compilation_CSG16 -o compil_rol.coal -c rol 3
	./bin/filecoder tests/Compilation_CSG16 -o compil_xor.coal -c xor alsdfuigawef2430-tr78
	./bin/filedecoder test1.coal -o Test1_plain
	./bin/filedecoder test1_rol.coal -o Test1_ROL
	./bin/filedecoder test1_xor.coal -o Test1_XOR
	./bin/filedecoder compil.coal -o compil_plain
	./bin/filedecoder compil_rol.coal -o compil_ROL
	./bin/filedecoder compil_xor.coal -o compil_XOR

clean:
	rm bin/*
	rm src/*.o
