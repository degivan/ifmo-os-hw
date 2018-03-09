all: simplesh

simplesh.o: simplesh.cpp
	g++ -std=c++11 -c simplesh.cpp -o simplesh.o

simplesh: simplesh.o
	g++ -std=c++11 -s simplesh.o -o simplesh

clean:
	$(RM) simplesh simplesh.o