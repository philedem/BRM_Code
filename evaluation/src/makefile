
main: clean
	gcc -DSHIFTOR -o main main.c tpool.c -lgmp -lpthread

and: clean
	gcc -o main main.c tpool.c -lgmp -lpthread

or2: clean
	gcc -DSHIFTOR -DCONSTRAINTS1 -o main main.c tpool.c -lgmp -lpthread

or3: clean
	gcc -DSHIFTOR -DCONSTRAINTS2 -o main main.c tpool.c -lgmp -lpthread

read: clean
	gcc -DREADCAND -o main main.c tpool.c -lgmp -lpthread


debug: clean
	gcc -DSHIFTOR -DDEBUG -DDEBUG_SEARCH -DINC_INSERT -o main main.c tpool.c -lgmp -lpthread

clean:
	rm -f main *.lib

