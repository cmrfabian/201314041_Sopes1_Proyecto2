all: juego

juego: juego.o
	gcc -g -o juego juego.o -lncurses -pthread -Wall

juego.o: juego.c
	gcc -o juego.o -c juego.c -Wall

.PHONY: clean
clean:
	rm -f *.o
	rm -f juego
