CC = gcc
CFLAGS = -Wall -Wextra -g
LIBS = -lm
OBJS = main.o myGet.o myGetopt.o libhttp.o

all: httpServer

httpServer: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o httpServer $(LIBS)

main.o: main.c myGet.h myGetopt.h
	$(CC) $(CFLAGS) -c main.c

myGet.o: myGet.c myGet.h
	$(CC) $(CFLAGS) -c myGet.c

myGetopt.o: myGetopt.c myGetopt.h
	$(CC) $(CFLAGS) -c myGetopt.c

clean:
	rm -f *.o myprogram
