all: prime mqprime

prime: prime.c
	gcc -g -o prime prime.c 

mqprime: mqprime.c
	gcc -Wall -g -o mqprime mqprime.c -lrt

clean:
	rm prime
	rm mqprime
