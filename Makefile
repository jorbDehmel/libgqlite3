.PHONY:	install
install:	src/gql.hpp
	sudo cp src/*.hpp /usr/include

.PHONY:	check
check:
	@echo "Checking for libsqlite3-dev..."
	@find /usr/include -type f -iname "sqlite3.h" | grep / > /dev/null

	@echo "Checking for C++11..."
	@g++ -std=c++11 -D DESIRED_VERSION=201100ULL -c -o /dev/null test/assert_version.cpp > /dev/null

	@echo "Checking for C++20..."
	@g++ -std=c++20 -D DESIRED_VERSION=202000ULL -c -o /dev/null test/assert_version.cpp > /dev/null

	@echo "Checking for dot (graphviz)..."
	@dot --version > /dev/null

	@echo "Environment is valid."

.PHONY:	format
format:
	find . \( -iname '*.cpp' -or -iname '*.hpp' \) \
		-exec clang-format -i {} \;

.PHONY:	test
test:
	$(MAKE) -C test
	$(MAKE) -C cli test

.PHONY: docs
docs:
	doxygen -q
	$(MAKE) -C latex
	cp latex/refman.pdf refman.pdf

.PHONY:	clean
clean:
	rm -rf html/ latex/ refman.pdf
	find . \( -iname '*.o' -or -iname '*.out' -or \
		-iname '*.db' -or -iname '*.db-journal' -or \
		-iname '*.dot' -or -iname '*.png' \) \
		-exec rm -f "{}" \;
