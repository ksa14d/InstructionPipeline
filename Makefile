CC = g++
CFLAGS= -O0 -Wall -std=c++11 -pedantic
LIBS = -lpthread\

all: pipe 

clean:
	rm -f *.o
	rm -f pipe
	rm -f *~
	rm -f core

pipe.o: pipe.cpp
	$(CC) $(CFLAGS) -g -c pipe.cpp

pipe:  pipe.o
	$(CC) $(CFLAGS) -g -o pipe pipe.o      

demoCache: 
	./pipe < physical.dat
demoTLB: 
	./pipe < trace.dat
