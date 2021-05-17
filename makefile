
main: clean
	gcc -DSHIFTOR -o main main.c -lgmp

no_insert: clean
	gcc -DSHIFTOR -DDEBUG -o main main.c -lgmp

debug: clean
	gcc -DDEBUG -DDEBUG_SEARCH -DSHIFTOR -o main main.c -lgmp

shiftand: clean
	gcc -DINC_INSERT -o main_and main.c -lgmp

clean:
	rm -f main *.lib

