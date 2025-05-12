.PHONY:	install
install:	src/gql.hpp
	sudo cp src/*.hpp /usr/include
	sudo ln -sf /usr/include/gql.hpp /usr/include/gqlite3.hpp

.PHONY:	check
check:
	@echo "Checking for libsqlite3-dev..."
	@find /usr/include -type f -iname "sqlite3.h" | grep / > /dev/null

	@echo "Checking for C++11..."
	@g++ -std=c++11 -D DESIRED_VERSION=201100ULL -c -o /dev/null test/assert_version.cpp > /dev/null

	@echo "Checking for C++20..."
	@g++ -std=c++20 -D DESIRED_VERSION=202000ULL -c -o /dev/null test/assert_version.cpp > /dev/null

	@echo "Checking for dot (graphviz)..."
	@which dot > /dev/null

	@echo "Checking for clang-format (optional)..."
	@if ! which clang-format > /dev/null ; then \
		echo "Missing optional dependency clang-format!" ; \
	fi

	@echo "Checking for clang-tidy (optional)..."
	@if ! which clang-tidy > /dev/null ; then \
		echo "Missing optional dependency clang-format!" ; \
	fi

	@echo "Environment is valid."

.PHONY:	format
format:
	find . \( -iname '*.cpp' -or -iname '*.hpp' \) \
		-exec clang-format -i {} \;

.PHONY:	tidy
tidy:
	find . \( -iname '*.cpp' -or -iname '*.hpp' \) \
		-and -not -iname 'assert_version.cpp' \
		-exec clang-tidy -extra-arg=-std=c++20 {} \;

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
