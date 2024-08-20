TESTS := test.out readme_ex.out traversal.out make.out \
	stress_test.out cpg.out
FORMAT_TESTS := $(TESTS:%.out=%.format.out)

CC := g++ -std=c++20
LFLAGS := -pedantic -Wall -g -O3
CFLAGS := $(LFLAGS)
LIBS := -lsqlite3

.PHONY:	all
all:	$(TESTS) $(FORMAT_TESTS)
	for test in $(TESTS) ; do 	\
		./$$test ;				\
	done

	for test in $(FORMAT_TESTS) ; do 	\
		./$$test ;				\
	done

%.out:	%.o
	$(CC) $(LFLAGS) -o $@ $^ $(LIBS)

%.format.out:	%.format.o
	$(CC) $(LFLAGS) -o $@ $^ $(LIBS)

%.o:	%.cpp
	$(CC) $(CFLAGS) -c -o $@ $^

%.format.o:	%.cpp
	$(CC) -D FORCE_CUSTOM_FORMAT=true $(CFLAGS) -c -o $@ $^

.PHONY:	clean
clean:
	rm -f *.o *.out *.dot *.db
