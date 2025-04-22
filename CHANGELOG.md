
# GQL Changelog

The `C++` definition `GQL_VERSION` contains a 9+ digit code
representing the GQL version. The three least significant digits
are the patch version, the next three are the minor version, and
the remaining digits are the major version. For instance, the
code `000000003` is equivalent to `0.0.3` and the code
`123456001` is `123.456.1.`

## `0.2.1` (4/22/2025)
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
- Added doxygen to `gql.hpp`

## `0.0.5`
- Added "bouncing" to avoid stack overflows in sqlite
- Added bounce test in unit testing

## `0.0.4`
- Added `merge_rows` for results objects, allowing easier
    aggregation
- Fixed some issues causing g++ pedantic warnings about
    signedness
- Removed some vestigial "depth" members
- Added "limit" method to limit the number of results (sorted
    by id)
- Added "excluding" as an easier version of "complement"
- Added multi-key "tag" query
- Added "lemma" for executing arbitrary lambdas before
    continuing in the query
- Added more unit tests

## `0.0.3`
- Began changelog
- Added `in_degree` and `out_degree` queries
- Added `cli` integration test (this is also a functional
    interpretter w/ variables and queries)
