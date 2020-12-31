
main: clean
	gcc -DINC_INSERT -o main main.c -lgmp

no_insert: clean
	gcc -o main main.c -lgmp

debug: clean
	gcc -DDEBUG -o main main.c -lgmp

shiftor: clean
	gcc -DSHIFTOR -DDEBUG -o main_or main.c -lgmp

clean:
	rm -f main *.lib

