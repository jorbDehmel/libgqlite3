
# GQL Changelog

The `C++` definition `GQL_VERSION` contains a 9+ digit code
representing the GQL version. The three least significant digits
are the patch version, the next three are the minor version, and
the remaining digits are the major version. For instance, the
code `000000003` is equivalent to `0.0.3` and the code
`123456001` is `123.456.1.`

## `0.3.3`
- Added cast to `std::string` from `GQL::Result`
- Added `empty()` and `exists()` functions for vertices and
    edges

## `0.3.2`
- Fixed accidental lack of `break` in switch statement in
    `GQL::graphviz::sanitize` which caused quote issues and
    possibly malformed output

## `0.3.1`
- Improved Doxygen

## `0.3.0`
- Improved documentation for parameters and return values
- Removed injection-vulnerable `in_degree` and `out_degree`
    arguments
- Move injection-vulnerable constructors to protected
- Made hex decoding and encoding throw in invalid cases
- Improved label and tag hex encoding to reduce injection
    vulnerabilities
- Removed `GQL::dump`
- Fixed outdated README example

## `0.2.5`
- Fixed bug causing non-ASCII characters in graphviz files

## `0.2.4`
- Added optional `clang-format` and `clang-tidy` dependencies in
    root `make check` for clarity
- Removed useless `constexpr` keywords in constructors

## `0.2.3`
- Minor documentation modifications
- Made CLI better, added CLI README.md

## `0.2.2`
- Added getter for file path
- Moved CLI into own folder
- Expanded & refined CLI

## `0.2.1`
- Changed from `Microsoft` to `LLVM` `clang-format` style
- Added `Doxyfile`, improved Doxygen support
- Changed default database path to `:memory:` (memory-only, no
    disk usage)
- Added checks for database file existence before any erasures

## `0.2.0`
- Replaced unsafe exposed SQL clauses w/ lambdas
- Increased safety in interface
- Fixed tests to reflect new interface

## `0.1.0`
- Fixed some pedantic compiler deprecation warnings w/ copy
    constructors in `Vertices` and `Edges`
- Reformatted testing Makefile
- Added `make check` for environment checking
- Added Doxygen to `gql.hpp`

## `0.0.5`
- Added "bouncing" to avoid stack overflows in SQLite3
- Added bounce test in unit testing

## `0.0.4`
- Added `merge_rows` for results objects, allowing easier
    aggregation
- Fixed some issues causing g++ pedantic warnings about
    sign-edness
- Removed some vestigial "depth" members
- Added "limit" method to limit the number of results (sorted
    by ID)
- Added "excluding" as an easier version of "complement"
- Added multi-key "tag" query
- Added "lemma" for executing arbitrary lambdas before
    continuing in the query
- Added more unit tests

## `0.0.3`
- Began changelog
- Added `in_degree` and `out_degree` queries
- Added `cli` integration test (this is also a functional
    interpreter w/ variables and queries)
