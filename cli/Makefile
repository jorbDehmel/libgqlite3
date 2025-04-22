CPP = g++ -pedantic -Wall -O3 -g -std=c++20

cli.out:	cli.o
	$(CPP) -o $@ $^ -lsqlite3

cli.o:	cli.cpp ../src/gql.hpp
	$(CPP) -c -o $@ $<

.PHONY:	test
test:	cli.out cli_input.txt
	./cli.out --input cli_input.txt
	cat cli_input.txt | ./cli.out
