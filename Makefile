default: paout

clean:
	rm -rf obj/ paout

obj/main.o: src/main.c
	mkdir -p obj/
	gcc -c src/main.c -O2 -o obj/main.o

paout: obj/main.o
	gcc obj/main.o -o paout -lpulse
