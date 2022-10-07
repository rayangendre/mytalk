CFLAGS = -Wall -g -I ~pn-cs357/Given/Talk/include
LDFLAGS = -L ~pn-cs357/Given/Talk/lib64

mytalk: main.o
	gcc -L ~pn-cs357/Given/Talk/lib64 -o mytalk main.o -l talk -l ncurses

main.o: main.c
	gcc -Wall -g -I ~pn-cs357/Given/Talk/include -c -o main.o main.c

clean:
	rm *.o
	echo done
