CFLAGS:= --std=c++11
test:
	g++ -o ./out/cbchain.o -g -c  $(CFLAGS) cbchain.cpp
	g++ -o test.out -g $(CFLAGS) test.cpp ./out/cbchain.o

