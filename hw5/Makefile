all: rshd

rshd.o: rshd.cpp
	g++ -std=c++11 -c rshd.cpp -o rshd.o

rshd: rshd.o
	g++ -std=c++11 -s rshd.o -o rshd

clean:
	$(RM) rshd rshd.o