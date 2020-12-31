
main: clean
	gcc -DINC_INSERT -o main.exe main.c -lgmp

no_insert: clean
	gcc -o main.exe main.c -lgmp

debug: clean
	gcc -DDEBUG -o main.exe main.c -lgmp

clean:
	rm -f main.exe *.lib

