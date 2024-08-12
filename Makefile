.PHONY:	install
install:	src/gql.hpp
	sudo cp src/*.hpp /usr/include

.PHONY:	format
format:
	find . \( -iname '*.cpp' -or -iname '*.hpp' \) \
		-exec clang-format -i {} \;

.PHONY:	test
test:
	$(MAKE) -C test

.PHONY:	clean
clean:
	find . \( -iname '*.o' -or -iname '*.out' -or \
		-iname '*.db' -or -iname '*.dot' \) -exec rm -f "{}" \;
