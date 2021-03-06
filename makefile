
main: clean
	gcc -DSHIFTOR -DINC_INSERT -o main main.c -lgmp

no_insert: clean
	gcc -DSHIFTOR -DDEBUG -o main main.c -lgmp

debug: clean
	gcc -DSHIFTOR -DDEBUG -DINC_INSERT -o main main.c -lgmp

shiftand: clean
	gcc -DDEBUG -o main_or main.c -lgmp

clkit: clean
	gcc -DCLKIT -DSHIFTOR -DINC_INSERT -o main main.c -lgmp

clean:
	rm -f main *.lib

