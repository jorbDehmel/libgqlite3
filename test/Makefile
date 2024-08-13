TESTS := test.out readme_ex.out traversal.out make.out \
	stress_test.out

CC := g++ -std=c++20
LFLAGS := -pedantic -Wall -g -O3
CFLAGS := $(LFLAGS)
LIBS := -lsqlite3

.PHONY:	all
all:	$(TESTS)
	for test in $(TESTS) ; do 	\
		./$$test ;				\
	done

%.out:	%.o
	$(CC) $(LFLAGS) -o $@ $^ $(LIBS)

%.o:	%.cpp
	$(CC) $(CFLAGS) -c -o $@ $^

.PHONY:	clean
clean:
	rm -f *.o *.out *.dot *.db
