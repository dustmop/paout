paout: src/main.c
	mkdir -p obj/
	gcc -c src/main.c -o obj/main.o
	gcc obj/main.o -o paout -lpulse
